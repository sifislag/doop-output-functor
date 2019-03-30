#include <string.h>
//#include <string>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <pthread.h>
#include <thread>

using std::ofstream;
using std::cout;
using std::endl;
using std::string;
using std::to_string;

#define BUFFER_SIZE 4096
#define MAX_NUM_OF_THREADS 32

class BufferedWriter{
  public:  
    char buffer[BUFFER_SIZE];
    int charsInBuffer = 0;
    //string outFileName;
    ofstream** outStreamPtr;
    pthread_mutex_t* myMutex;
    BufferedWriter** otherBuffersSameFile;
    int disabled;

    BufferedWriter(pthread_mutex_t* mut, ofstream** streamPtr, string fileName, BufferedWriter** bufferArr){
    	cout << "MY CONSTRUCTOR FROM " <<  std::this_thread::get_id() << endl;
    	disabled = 1;
    	myMutex = mut;
    	outStreamPtr = streamPtr;
    	otherBuffersSameFile = bufferArr;
    	initFile(fileName);
    }

    ~BufferedWriter(){
		pthread_mutex_lock(myMutex);
		cout << "MYDESTRUCTOR FROM " <<  std::this_thread::get_id() << endl;
		//cout << "Numofthreads " << numOfThreads << endl;
		
		flush();
		for(int i=0; i < MAX_NUM_OF_THREADS; i++)
			if(otherBuffersSameFile[i]!=NULL)
				otherBuffersSameFile[i]->flush();
		
		//if(--numOfThreads == 0)
		(*outStreamPtr)->close();
		
		pthread_mutex_unlock(myMutex);
	}

	void initFile(string fileName){
		pthread_mutex_lock(myMutex);
		if(*outStreamPtr == NULL){
			char* outFolder = getenv("ANALYSIS_OUT");
			if(outFolder == NULL){
				cout << "ANALYSIS_OUT env variable doesn't exist" << endl;
				return;
			}

			disabled=0;
			string outFile = outFolder + fileName; 
			cout << "OPENED FILE " << outFile << endl;
			*outStreamPtr = new ofstream();
			(*outStreamPtr)->open(outFile);
			//numOfThreads = 1;
			otherBuffersSameFile[0] = this;
		}
		else{
			disabled=0;
			for(int i=0; i < MAX_NUM_OF_THREADS; i++){
				if(otherBuffersSameFile[i]!=NULL){
					continue;
				}
				else{
					otherBuffersSameFile[i] = this;
					break;
				}
			}
		}
		pthread_mutex_unlock(myMutex);	
    }

    void flush(){
    	if(charsInBuffer != 0 && *outStreamPtr != NULL)
    		(*outStreamPtr)->write(buffer, charsInBuffer);
    	charsInBuffer=0;
    }

    void addString(string content){
    	if(disabled)
    		return;
        int stringLen = content.length();
        if(charsInBuffer + stringLen < BUFFER_SIZE){
        	//cout << "APPENDING TO BUFFER FROM "<< std::this_thread::get_id() <<endl;
        	strcat(buffer , content.c_str());
        	charsInBuffer += stringLen;
        }
        else{
        	pthread_mutex_lock(myMutex);
        	//cout << "EMPTYING BUFFER FROM "<< std::this_thread::get_id() <<endl;
        	(*outStreamPtr)->write(buffer, charsInBuffer);
        	(*outStreamPtr)->write(content.c_str(), stringLen);
        	buffer[0] = '\0';
        	charsInBuffer = 0;
        	pthread_mutex_unlock(myMutex);
        }
        //cout << content;
    }
};

static pthread_mutex_t vptMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cgeMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t otherMutex = PTHREAD_MUTEX_INITIALIZER;

static ofstream* vptFile = NULL;
static ofstream* cgeFile = NULL;
static ofstream* otherFile = NULL;


static BufferedWriter* vptBufferArray[MAX_NUM_OF_THREADS];
static BufferedWriter* cgeBufferArray[MAX_NUM_OF_THREADS];
static BufferedWriter* otherBufferArray[MAX_NUM_OF_THREADS];


thread_local static BufferedWriter vptBuffer(&vptMutex, &vptFile, "/liveVPT.csv", vptBufferArray);
thread_local static BufferedWriter cgeBuffer(&cgeMutex, &cgeFile, "/liveCGE.csv", cgeBufferArray);
thread_local static BufferedWriter otherBuffer(&otherMutex, &otherFile, "/liveOTher.csv", otherBufferArray);

extern "C" {

	int32_t logVPT(int32_t hctx, const char *val, int32_t ctx, const char *var) {
		string stringToAppend = to_string(hctx) + '\t' + val + '\t' + to_string(ctx) + '\t' + var + '\n';
		vptBuffer.addString(stringToAppend);
		return 0;
	}

	int32_t logCGE(int32_t callerCtx, const char *invo, int32_t calleeCtx, const char *toMeth) {
		string stringToAppend = to_string(callerCtx) + '\t' + invo + '\t' + to_string(calleeCtx) + '\t' + toMeth + '\n';
		cgeBuffer.addString(stringToAppend);
		return 0;
	}

	int32_t logOther(const char *otherStr) {
		string stringToAppend = otherStr + '\n';
		cgeBuffer.addString(stringToAppend);
		return 0;
	}

}