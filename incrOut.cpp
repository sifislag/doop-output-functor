#include <string.h>
//#include <string>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <pthread.h>
#include <thread>
#include <set>
#include <unordered_map>
#include <iterator>
#include <sstream>

using std::ofstream;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::set;
using std::unordered_map;
using std::pair;

#define BUFFER_SIZE 4096
#define MAX_NUM_OF_THREADS 32

class BufferedWriter{
  public:  
    char *buffer;
    int charsInBuffer = 0;
    //string outFileName;
    ofstream** outStreamPtr;
    pthread_mutex_t* myMutex;
    BufferedWriter** otherBuffersSameFile;
    int disabled;

    BufferedWriter(pthread_mutex_t* mut, ofstream** streamPtr, string fileName, BufferedWriter** bufferArr, int bufferSize){
    	cout << "MY CONSTRUCTOR " + fileName + " FROM " <<  std::this_thread::get_id() << endl;
    	disabled = 1;
    	myMutex = mut;
    	outStreamPtr = streamPtr;
    	otherBuffersSameFile = bufferArr;
    	buffer = (char*)malloc(sizeof(char) * bufferSize);
    	buffer[0] = '\0';
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

class RelationEntry{
  public:	
	string relName;
	ofstream* fileStream;
	pthread_mutex_t relMutex;
	BufferedWriter* bufferArray[MAX_NUM_OF_THREADS];

	RelationEntry(string name){
		relName = name;
		fileStream = NULL;
		for(int i=0; i < MAX_NUM_OF_THREADS; i++)
			bufferArray[i]=NULL;			
		//relMutex = PTHREAD_MUTEX_INITIALIZER;
		int rc = pthread_mutex_init(&relMutex, NULL);
		if(rc != 0){
			cout << "Mutex initialization failed." << endl;
		}
	}

	bool operator< (const RelationEntry & rel) const{
		return this-> relName < rel.relName;
	}

};

struct RelEntryPtrComp
{
    bool operator()(const RelationEntry* lhs, const RelationEntry* rhs) const  { 
    	return lhs->relName < rhs->relName;
    }
};

static pthread_mutex_t globMutex = PTHREAD_MUTEX_INITIALIZER;

static RelationEntry liveVPTEntry("liveVPT");
static RelationEntry liveCGEEntry("liveCGE");
static RelationEntry liveFPTEntry("liveFPT");

static unordered_map<string, RelationEntry*> outputRelations{
	pair<string, RelationEntry*>("liveVPT", &liveVPTEntry), 
	pair<string, RelationEntry*>("liveCGE",&liveCGEEntry), 
	pair<string, RelationEntry*>("liveFPT", &liveFPTEntry)
};

thread_local static unordered_map<string, BufferedWriter*> threadsBuffers;

thread_local static BufferedWriter vptBuffer(&(liveVPTEntry.relMutex),&(liveVPTEntry.fileStream), "/"+liveVPTEntry.relName+".csv", liveVPTEntry.bufferArray, BUFFER_SIZE);
thread_local static BufferedWriter cgeBuffer(&(liveCGEEntry.relMutex),&(liveCGEEntry.fileStream), "/"+liveCGEEntry.relName+".csv", liveCGEEntry.bufferArray, BUFFER_SIZE);
thread_local static BufferedWriter fptBuffer(&(liveFPTEntry.relMutex),&(liveFPTEntry.fileStream), "/"+liveFPTEntry.relName+".csv", liveFPTEntry.bufferArray, BUFFER_SIZE);


    void fflushAllBuffers(){
    	unordered_map<string, RelationEntry*>::iterator itr;
	    for (itr = outputRelations.begin(); itr != outputRelations.end(); ++itr) {
	    	RelationEntry* entry = itr->second;
	    	for(int i=0; i < MAX_NUM_OF_THREADS; i++)
				if(entry->bufferArray[i]!=NULL)
					entry->bufferArray[i]->flush();
	    }
    }


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

	int32_t logFPT(const char *otherStr) {
		string stringToAppend = otherStr + '\n';
		fptBuffer.addString(stringToAppend);
		return 0;
	}

	int32_t logRel(const char *liveRel, const char *otherStr) {
		auto relBuffer = threadsBuffers.find(liveRel); 
		if(relBuffer == threadsBuffers.end()){
			auto relEntry = outputRelations.find(liveRel);
			if(relEntry == outputRelations.end()){
				pthread_mutex_lock(&globMutex);
				outputRelations.insert(pair<string, RelationEntry*>(liveRel, new RelationEntry(liveRel)));
				relEntry = outputRelations.find(liveRel);
				//cout << "Adding new relation: " << liveRel << endl;
				pthread_mutex_unlock(&globMutex);
			}
			//cout << "Adding new relation: " << liveRel << " in thread " << std::this_thread::get_id() << endl;
			threadsBuffers.insert(pair<string, BufferedWriter*>(liveRel, new BufferedWriter(&(relEntry->second->relMutex),&(relEntry->second->fileStream), "/"+relEntry->second->relName+".csv", relEntry->second->bufferArray, BUFFER_SIZE)));
			relBuffer = threadsBuffers.find(liveRel);
		}
		string stringToAppend = otherStr;
		relBuffer->second->addString(stringToAppend+ "\n");
		fptBuffer.addString(stringToAppend);
		return 0;
	}

	int32_t registerAtExitFunction(int32_t callerCtx){
		atexit(fflushAllBuffers);
		return 0;
	}

}