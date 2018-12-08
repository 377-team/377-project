#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <time.h>
#include <stdint.h> //used so that on any platform we are working with 16bit integers for the addresses (uint16_t)
#include <thread>
#include <mutex>
using namespace std;
#define inBuffer 10 //size of the buffer set by description of the problem
#define page_array_size 8 //size of the page array it is 8 based on how much memory the system has and the size of the "pages"
#define test_data_size 10000 //change this to 10000 for "production" run
uint16_t inBuffArray[inBuffer]; // buffer array set to size of the inBuffer
int inCount_of_Buffer = 0; //counter for the number of addresses currently in the buffer
int totalPageFaults = 0; //counter for the total number of page faults
bool agNotComplete = true; //boolean variable for the completion of address generation
ofstream printFile; //declare the output stream
mutex bufferMutex; //mutex used to lock and unlock the buffer

//class for the generation of addresses
class addressGenerator
{
    public:
        addressGenerator()
        {

        }
        void generateAddresses()
        {
            int cntTestData = test_data_size; //set the counter that will be decremented in the while loop to the same size as test_data_size
            while(cntTestData > 0)
            {
                if(inCount_of_Buffer < inBuffer)
                {
                    newAddr = NextVirtualAddress(); //assign randomly generated value to temp variable
                    cntTestData--;
                    bufferMutex.lock(); //lock the shared memory so the translate_values thread can not access this while this thread is using inCount_of_Buffer
                    inBuffArray[inCount_of_Buffer] = newAddr; //store new randomly generated 16 bit value to the buffer array
                    inCount_of_Buffer++; // increment the buffers check counter
                    bufferMutex.unlock(); //unlock the shared memory
                }
                else
                {
                   this_thread::yield(); //have thread wait until space frees up in the buffer
                }

            }
            agNotComplete = false;
        }

      //handles the threading of the class as well as checking the buffer status and adding addresses to it as needed
    private:
        int NextVirtualAddress() //actually generates the random values stored in the addresses
        {
            uint16_t nxtAddr = 0;
            nxtAddr = rand() % 65536; //generates a random 16-bit integer and stores it in nxtAddr
            return nxtAddr;
        }
        uint16_t newAddr;
};

//class that handles the translation of the virtual addresses to physical addresses as well as the paging algorithm
class addressTranslator
{
    public:
        //constructor
        addressTranslator()
        {

        }
        void translator()
        {
                //still add in file
                nxtPage = 0;
                while(agNotComplete)
                {
                    if(inCount_of_Buffer <= 0)
                    {
                        this_thread::yield(); //halt the thread because there is no more addresses in the buffer to translate
                    }
                    while(inCount_of_Buffer > 0)
                    {
                        bufferMutex.lock(); //lock the shared memory so the make_values thread can not access this while this thread is using inCount_of_Buffer
                        inCount_of_Buffer--; //decrement the inCount_of_Buffer to point to a valid element in the array
                        current_address = inBuffArray[inCount_of_Buffer];
                        bufferMutex.unlock();
                        physical_address = transAddress(current_address);
                        //write the virtual and physical address to a file
                        printFile << "0x" << hex << setfill('0') << setw(4) << current_address << " -> 0x" << hex << setfill('0') << setw(4) << physical_address << endl;

                    }
                }
        }
    private:
        uint16_t transAddress(uint16_t transAddr)
        {
            uint16_t tempPhys_address; //temp variable for storing the physical address
            uint16_t temp_Offset; // temp variable for storing the offset which is used to determine is a page is in the page table or not
            uint16_t offsetMask = 2047; // this is the mask used to and with tempPhys_address to find the
            bool pageMiss;
            tempPhys_address = transAddr;
            tempPhys_address &= offsetMask; //and the current address being translated to get the number of page faults
            temp_Offset = transAddr;
            temp_Offset >>= 11;
            pageMiss = true; //assume page miss on start
            for(int i = 0; i < 8; i++) //check contents of the "page table"
            {
                if(temp_Offset == page_arry[i]) //check for a "page" hit
                {
                    pageMiss = false; //if there is a hit change the pageMiss boolean
                }
            }
            if(pageMiss) //on page miss
            {
                page_arry[nxtPage] = temp_Offset; //add the "page" to "page table"
                nxtPage++; //increment counter to point to next open slot
                totalPageFaults++; //increment counter for total number of page faults
                if(nxtPage == 8) //if "page table" is full reset nxtPage to index zero to "point" to the first entry entered in thus the first entry to evict
                {
                    nxtPage = 0;
                }
            }
            return tempPhys_address;
        }
        uint16_t current_address;
        uint16_t physical_address;
        uint16_t page_arry[page_array_size] = {0,0,0,0,0,0,0,0};
        int nxtPage = 0;
};

class reporter
{
    public:
        reporter()
        {

        }
        void divulge(void *ptr)
        {

            thread* ptrFromAT = (thread*)ptr;//type cast pointer to the translate_values thread
            ptrFromAT->join(); //join on the translate_values thread. Blocks until the translate_values thread exits then continues executing this (divulge) thread

            printFile <<"The total number of page faults is: " << dec << totalPageFaults << " when using FIFO" << endl;
            cout<<"The total number of page faults is: " << dec << totalPageFaults << " when using FIFO" << endl;

        }
};

int main()
{
    printFile.open("377_simulator_results.txt",ios::trunc); //open the file, if it doesn't exist this will create it. ios::trunc sets the file to overwrite previous content
    if(printFile.is_open()) //verify we can write to the file
    {
        printFile << "This is a virtual memory simulator that uses a FIFO based paging algorithm. \n";
        printFile << setw(4) << "Virtual " <<  setw(4) << " Physical" << endl;
    }
    else //if not close the program before the threads are started
    {
        exit(EXIT_FAILURE);
    }

    time_t sys_time = time(NULL); //create time based variable to use as seed
    srand(sys_time); //seed the rand() function


    //instantiate one of each class
    addressGenerator AG;
    addressTranslator AT;
    reporter RP;

    //spawn threads
    thread make_values(&addressGenerator::generateAddresses,&AG); //thread for generating addresses
    thread translate_values(&addressTranslator::translator,&AT); //thread for translating the addresses
    thread divulge(&reporter::divulge,&RP, &translate_values); //thread for reporting. Passes the address of the translate_values thread to
    make_values.join();
    translate_values.join();
    divulge.join();
    return 0;
}


