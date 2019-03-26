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

void initFile();

static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static ofstream* vptFile = NULL;
static int numOfThreads = 0;

class BufferedWriter{
  public:  
    char buffer[BUFFER_SIZE];
    int charsInBuffer = 0;
    ofstream* outStream = NULL;
    int disabled = 0;

    ~BufferedWriter();

    void flush(){
    	if(charsInBuffer != 0 && outStream != NULL)
    		outStream->write(buffer, charsInBuffer);
    	charsInBuffer=0;
    }

    void addString(string content){
    	if(disabled)
    		return;
        if(outStream == NULL){
            initFile();
            if(vptFile == NULL)
                disabled = 1;
            else
            	outStream = vptFile;
        }
        int stringLen = content.length();
        if(charsInBuffer + stringLen < BUFFER_SIZE){
        	//cout << "APPENDING TO BUFFER FROM "<< std::this_thread::get_id() <<endl;
        	strcat(buffer , content.c_str());
        	charsInBuffer += stringLen;
        }
        else{
        	pthread_mutex_lock(&count_mutex);
        	//cout << "EMPTYING BUFFER FROM "<< std::this_thread::get_id() <<endl;
        	outStream->write(buffer, charsInBuffer);
        	outStream->write(content.c_str(), stringLen);
        	buffer[0] = '\0';
        	charsInBuffer = 0;
        	pthread_mutex_unlock(&count_mutex);
        }
        //cout << content;
    }
};

thread_local static BufferedWriter myBuffer;
static BufferedWriter* bufferArr[32];

void initFile(){
	pthread_mutex_lock(&count_mutex);
	if(vptFile == NULL){
		char* outFolder = getenv("ANALYSIS_OUT");
		if(outFolder == NULL)
			cout << "ANALYSIS_OUT env variable doesn't exist" << endl;
		string outFile = outFolder + string("/liveVPT.csv"); 
		cout << "OPENED FILE " << outFile << endl;
		vptFile = new ofstream();
		vptFile->open(outFile);
		numOfThreads = 1;
		bufferArr[0] = &myBuffer;
	}
	else{
		bufferArr[numOfThreads] = &myBuffer;
		numOfThreads++;
	}
	pthread_mutex_unlock(&count_mutex);
}

BufferedWriter::~BufferedWriter(){
	pthread_mutex_lock(&count_mutex);
	cout << "MYDESTRUCTOR FROM " <<  std::this_thread::get_id() << endl;
	cout << "Numofthreads " << numOfThreads << endl;
	
	flush();
	for(int i=0; i<numOfThreads; i++)
		bufferArr[i]->flush();
	
	//if(--numOfThreads == 0)
	outStream->close();
	
	pthread_mutex_unlock(&count_mutex);
}

extern "C" {

	int32_t logVPT(int32_t hctx, const char *val, int32_t ctx, const char *var) {
		string stringToAppend = to_string(hctx) + "\t" + val + "\t" + to_string(ctx) + "\t" + var + "\n";
		myBuffer.addString(stringToAppend);
		return 0;
	}

}