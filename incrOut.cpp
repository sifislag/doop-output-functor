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
    string outFileName;
    int charsInBuffer = 0;
    ofstream** outStreamPtr;
    pthread_mutex_t* myMutex;
    BufferedWriter** otherBuffersSameFile;
    int disabled = 0;

    BufferedWriter(pthread_mutex_t* mut, ofstream** streamPtr, string fileName, BufferedWriter** bufferArr){
    	cout << "MY CONSTRUCTOR FROM " <<  std::this_thread::get_id() << endl;
    	myMutex = mut;
    	outStreamPtr = streamPtr;
    	outFileName = fileName;
    	otherBuffersSameFile = bufferArr;
    	initFile();
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

	void initFile(){
		pthread_mutex_lock(myMutex);
		if(*outStreamPtr == NULL){
			char* outFolder = getenv("ANALYSIS_OUT");
			if(outFolder == NULL)
				cout << "ANALYSIS_OUT env variable doesn't exist" << endl;
			string outFile = outFolder + outFileName; 
			cout << "OPENED FILE " << outFile << endl;
			*outStreamPtr = new ofstream();
			(*outStreamPtr)->open(outFile);
			//numOfThreads = 1;
			otherBuffersSameFile[0] = this;
		}
		else{
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

static pthread_mutex_t vpt_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cge_mutex = PTHREAD_MUTEX_INITIALIZER;
static ofstream* vptFile = NULL;
static ofstream* cgeFile = NULL;
static int numOfThreads = 0;
static BufferedWriter* vptBufferArray[MAX_NUM_OF_THREADS];
thread_local static BufferedWriter vptBuffer(&vpt_mutex, &vptFile, "/liveVPT.csv", vptBufferArray);

extern "C" {

	int32_t logVPT(int32_t hctx, const char *val, int32_t ctx, const char *var) {
		string stringToAppend = to_string(hctx) + "\t" + val + "\t" + to_string(ctx) + "\t" + var + "\n";
		vptBuffer.addString(stringToAppend);
		return 0;
	}

}