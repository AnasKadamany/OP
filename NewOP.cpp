#include <iostream>
#include <vector>
#include <map>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define DISK_SIZE 512
#define NUM_DIRECT_BLOCKS 3
// Function to convert decimal to binary char
char decToBinary(int n) {
    return static_cast<char>(n);
}

// #define SYS_CALL
// ============================================================================
class fsInode {
    int fileSize;
    int block_in_use;

    int directBlock1;
    int directBlock2;
    int directBlock3;
    int directBlocks[NUM_DIRECT_BLOCKS];
    int singleInDirect;
    int doubleInDirect;
    int block_size;


public:
    fsInode(int _block_size) {
        fileSize = 0;
        block_in_use = 0;
        block_size = _block_size;
        directBlock1 = -1;
        directBlock2 = -1;
        directBlock3 = -1;
        singleInDirect = -1;
        doubleInDirect = -1;

    }

    int getFileSize() const {
        return fileSize;
    }

    void setFileSize(int fileSize) {
        fsInode::fileSize = fileSize;
    }

    int getBlockInUse() const {
        return block_in_use;
    }

    void setBlockInUse(int blockInUse) {
        block_in_use = blockInUse;
    }

    int getDirectBlock1() const {
        return directBlock1;
    }
    int getDirectBlock(int index) {
        if (index < 0 || index >= NUM_DIRECT_BLOCKS) return -1;  // Error condition
        return directBlocks[index];
    }

    void setDirectBlock1(int directBlock1) {
        fsInode::directBlock1 = directBlock1;
    }

    int getDirectBlock2() const {
        return directBlock2;
    }

    void setDirectBlock2(int directBlock2) {
        fsInode::directBlock2 = directBlock2;
    }

    int getDirectBlock3() const {
        return directBlock3;
    }

    void setDirectBlock3(int directBlock3) {
        fsInode::directBlock3 = directBlock3;
    }

    int getSingleInDirect() const {
        return singleInDirect;
    }

    void setSingleInDirect(int singleInDirect) {
        fsInode::singleInDirect = singleInDirect;
    }

    int getDoubleInDirect() const {
        return doubleInDirect;
    }

    void setDoubleInDirect(int doubleInDirect) {
        fsInode::doubleInDirect = doubleInDirect;
    }

    int getBlockSize() const {
        return block_size;
    }

    void setBlockSize(int blockSize) {
        block_size = blockSize;
    }
    int getBlockNumberForOffset(int blockOffset) {
        if (blockOffset < 0) return -1;

        // Considering direct blocks
        if (blockOffset == 0) return directBlock1;
        if (blockOffset == 1) return directBlock2;
        if (blockOffset == 2) return directBlock3;

        // For other direct block pointers in the array
        if (blockOffset < 3 + NUM_DIRECT_BLOCKS) {
            return getDirectBlock(blockOffset - 3);
        }

        // If using single indirect or double indirect pointers,
        // logic will be more complicated and will require handling here

        return -1;  // Invalid block offset or not supported in this simple version
    }


};

// ============================================================================
class FileDescriptor {
    pair<string, fsInode*> file;
    bool inUse;
    fsInode* inodePtr;
    int currentPosition;
public:
    FileDescriptor(string FileName, fsInode* fsi) {
        file.first = FileName;
        file.second = fsi;
        inUse = true;
        inodePtr = fsi;  // Initialize inodePtr with the provided fsInode pointer
        currentPosition = 0;  // Initialize currentPosition to 0

    }

    const pair<string, fsInode *> &getFile() const {
        return file;
    }

    void setFile(const pair<string, fsInode *> &file) {
        FileDescriptor::file = file;
    }

    fsInode *getInodePtr() const {
        return inodePtr;
    }

    void setInodePtr(fsInode *inodePtr) {
        FileDescriptor::inodePtr = inodePtr;
    }

    int getCurrentPosition() const {
        return currentPosition;
    }

    void setCurrentPosition(int currentPosition) {
        FileDescriptor::currentPosition = currentPosition;
    }

    string getFileName() {
        return file.first;
    }

    fsInode* getInode() {

        return file.second;

    }

    int GetFileSize() {
        if(inodePtr) {
            return inodePtr->getFileSize(); // Assuming fsInode class has a method called getFileSize
        }
        return -1; // Error value, indicating that the inodePtr isn't initialized or some other error.
    }
    bool isInUse() {
        return (inUse);
    }
    void setInUse(bool _inUse) {
        inUse = _inUse ;
    }
};

#define DISK_SIM_FILE "DISK_SIM_FILE.txt"
// ============================================================================
class fsDisk {
    FILE *sim_disk_fd;
    int block_size;
    bool is_formated;

    // BitVector - "bit" (int) vector, indicate which block in the disk is free
    //              or not.  (i.e. if BitVector[0] == 1 , means that the
    //             first block is occupied.
    int BitVectorSize;
    int *BitVector;

    // Unix directories are lists of association structures,
    // each of which contains one filename and one inode number.
    map<string, fsInode*>  MainDir ;

    // OpenFileDescriptors --  when you open a file,
    // the operating system creates an entry to represent that file
    // This entry number is the file descriptor.
    vector< FileDescriptor > OpenFileDescriptors;
    vector<int> FreeFileDescriptorIndices;

public:
    // ------------------------------------------------------------------------
    fsDisk() {
        // Initialize the simulated disk file
        sim_disk_fd = fopen(DISK_SIM_FILE, "r+");
        assert(sim_disk_fd);

        for (int i = 0; i < DISK_SIZE; i++) {
            int ret_val = fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }
        fflush(sim_disk_fd);

        // Initialize BitVector
        // Initially, set the size of BitVectorSize as DISK_SIZE,
        // though this might change based on block size during formatting.
        BitVectorSize = DISK_SIZE;
        BitVector = new int[BitVectorSize];
        for (int i = 0; i < BitVectorSize; i++) {
            BitVector[i] = 0;  // 0 means the block is free
        }

        // Initialize Main Directory
        MainDir.clear();

        // Initialize OpenFileDescriptors vector
        OpenFileDescriptors.clear();

        // Set is_formated flag to false, as disk is not yet formatted
        is_formated = false;
    }
    ~fsDisk() {
        if (BitVector) {
            delete[] BitVector;
        }

        if (sim_disk_fd) {
            fclose(sim_disk_fd);
        }

        for (auto &entry : MainDir) {
            delete entry.second;
        }

        // The vector and map will automatically deallocate their memory, so no need for explicit deletion.
    }



    // ------------------------------------------------------------------------
    void listAll() {
        int i = 0;
        for ( auto it = begin (OpenFileDescriptors); it != end (OpenFileDescriptors); ++it) {
            cout << "index: " << i << ": FileName: " << it->getFileName() <<  " , isInUse: "
                 << it->isInUse() << " file Size: " << it->GetFileSize() << endl;
            i++;
        }
        char bufy;
        cout << "Disk content: '" ;
        for (i=0; i < DISK_SIZE ; i++) {
            int ret_val = fseek ( sim_disk_fd , i , SEEK_SET );
            ret_val = fread(  &bufy , 1 , 1, sim_disk_fd );
            cout << bufy;
        }
        cout << "'" << endl;


    }

    // ------------------------------------------------------------------------
    void fsFormat(int blockSize = 4, int direct_Enteris_ = 3) {
        this->block_size = blockSize;
        // 1. Reset the BitVector to indicate all blocks are free
        for (int i = 0; i < BitVectorSize; i++) {
            BitVector[i] = 0;  // 0 indicates the block is free
        }

        // 2. Clear the Main Directory
        // Before clearing, free up memory associated with each inode
        for (auto &entry : MainDir) {
            delete entry.second;
        }
        MainDir.clear();

        // 3. Clear OpenFileDescriptors
        OpenFileDescriptors.clear();

        // 4. Update the BitVectorSize based on the given blockSize
        // For simplicity, we assume the whole disk is dividable by blockSize without remainder.
        BitVectorSize = DISK_SIZE / blockSize;

        // Resize and reset the BitVector
        delete[] BitVector;  // delete the old BitVector
        BitVector = new int[BitVectorSize];
        for (int i = 0; i < BitVectorSize; i++) {
            BitVector[i] = 0;  // again, 0 indicates the block is free
        }

        // 5. Set is_formated to true
        is_formated = true;

        // Print a message to inform the user
        cout << "Disk formatted successfully with block size: " << blockSize << " and direct entries: " << direct_Enteris_ << endl;
    }

    // ------------------------------------------------------------------------
    int CreateFile(string fileName) {
        // 1. Check if the disk is formatted.
        if (!is_formated) {
            cout << "Error: Disk not formatted. Please format the disk first." << endl;
            return -1;
        }

        // 2. Verify if the file with the same name already exists.
        if (MainDir.find(fileName) != MainDir.end()) {
            cout << "Error: File with the name " << fileName << " already exists." << endl;
            return -1;
        }

        // 3. Allocate an fsInode for the new file.
        fsInode *newInode = new fsInode(block_size); // assuming block_size is a member variable of fsDisk.
        if (newInode == nullptr) {
            cout << "Error: Failed to allocate memory for new inode." << endl;
            return -1;  // or some error code
        }
        // 4. Direct, single and double indirect blocks are already set to -1 in the fsInode constructor.

        // 5. Add the filename and its inode pointer to the MainDir map.
        MainDir[fileName] = newInode;
        if (MainDir[fileName] == nullptr) {
            cout << "Error: Inode not stored correctly in MainDir." << endl;
        }
        // 6. Create a new file descriptor for the opened file.
        FileDescriptor newFD(fileName, newInode);
//        if (OpenFileDescriptors.back().getInodePtr() == nullptr) {
//            cout << "Error: Inode not stored correctly in OpenFileDescriptors." << endl;
//        }
        // Add it to OpenFileDescriptors and return its index.
        OpenFileDescriptors.push_back(newFD);
        int fd = OpenFileDescriptors.size() - 1; // last index as fiZZZle descriptor number
        std::cout << "Checking after CreateFile for FD: " << fd << std::endl;
        if(OpenFileDescriptors[fd].getInodePtr() == nullptr) {
            std::cout << "Error: Inode is null after file creation!" << std::endl;
        } else {
            std::cout << "Inode correctly assigned after file creation." << std::endl;
        }

        return fd;
    }

    // ------------------------------------------------------------------------
    int OpenFile(string FileName) {
        // Check if the file exists in MainDir
        if (MainDir.find(FileName) == MainDir.end()) {
            cout << "Error: File not found." << endl;
            return -1; // Error value
        }

        // Check if the file is already opened
        for (size_t i = 0; i < OpenFileDescriptors.size(); ++i) {
            if (OpenFileDescriptors[i].getFileName() == FileName && OpenFileDescriptors[i].isInUse()) {
                cout << "Error: File already opened." << endl;
                return -1; // Error value
            }
        }

        // Open the file
        fsInode* inode = MainDir[FileName];

        // Check if there's a free file descriptor to reuse
        int fd;
        if (!FreeFileDescriptorIndices.empty()) {
            fd = FreeFileDescriptorIndices.back();
            FreeFileDescriptorIndices.pop_back();

            // Reset the file descriptor to point to the new file
            OpenFileDescriptors[fd].setFile({FileName, inode});
            OpenFileDescriptors[fd].setInUse(true);
            OpenFileDescriptors[fd].setCurrentPosition(0);
        } else {
            // Create a new file descriptor and add it to the list
            FileDescriptor newFD(FileName, inode);
            OpenFileDescriptors.push_back(newFD);
            fd = OpenFileDescriptors.size() - 1;  // The index of the newly added descriptor
        }

        return fd;  // Returning the file descriptor
    }


    // ------------------------------------------------------------------------
    string CloseFile(int fd) {
        if (fd < 0 || fd >= OpenFileDescriptors.size() || !OpenFileDescriptors[fd].isInUse()) {
            cout << "Error: Invalid file descriptor or file not opened." << endl;
            return ""; // Error value
        }

        string fileName = OpenFileDescriptors[fd].getFileName();
        OpenFileDescriptors[fd].setInUse(false);
        FreeFileDescriptorIndices.push_back(fd);
        return fileName;
    }
    ///////////////////////////////////////////////
    int AllocateBlock() {
        for (int i = 0; i < BitVectorSize; i++) {
            if (BitVector[i] == 0) {  // Assuming 0 means the block is free
                BitVector[i] = 1;    // Mark the block as used
                return i;            // Return the block number
            }
        }
        cerr << "Error: No free blocks available." << endl;
        return -1;  // Return an error if no blocks are free
    }


    bool WriteToBlock(int blockNum, char* data, int len) {
        cout << "WriteToFile: Preparing to write \"" << data << "\" to block number " << blockNum << endl;

        if(blockNum < 0 || blockNum >= BitVectorSize) {
            cerr << "Error: Invalid block number provided." << endl;
            return false;
        }

        if(len <= 0 || len > block_size) {
            cerr << "Error: Invalid data length." << endl;
            return false;
        }

        // Display data in simulatedDisk before write operation
        std::cout << "Data in Block#" << blockNum << " before write: ";
        for (int i = 0; i < block_size; i++) {
            std::cout << simulatedDisk[blockNum][i] << " ";
        }
        std::cout << std::endl;

        // Write the data to the simulated disk
        if(fseek(sim_disk_fd, blockNum * block_size, SEEK_SET) != 0 || fwrite(data, sizeof(char), len, sim_disk_fd) != len) {
            cerr << "Error: Failed to write data to block." << endl;
            return false;
        }

        // Update in-memory representation (simulatedDisk) after the write operation to the file
        memcpy(simulatedDisk[blockNum], data, len);
        for(int i = len; i < block_size; i++) {
            simulatedDisk[blockNum][i] = '\0';  // Fill the rest with '\0' or appropriate padding
        }

        // DEBUG: Read back the written data and print for confirmation
        char debugBuffer[block_size];
        if (fseek(sim_disk_fd, blockNum * block_size, SEEK_SET) != 0 || fread(debugBuffer, sizeof(char), len, sim_disk_fd) != len) {
            cerr << "Error: Failed to read back data from block for debugging." << endl;
        } else {
            debugBuffer[len] = '\0'; // Ensure null-termination for printing
            cout << "DEBUG - Data written to Block#" << blockNum << ": " << debugBuffer << endl;
        }

        // Display data in simulatedDisk after write operation
        std::cout << "Data in Block#" << blockNum << " after write: ";
        for (int i = 0; i < block_size; i++) {
            std::cout << simulatedDisk[blockNum][i] << " ";
        }
        std::cout << std::endl;

        return true;
    }



    bool UpdateIndirectBlock(int blockNum, int index, int value) {
        if(blockNum < 0 || blockNum >= BitVectorSize) {
            cerr << "Error: Invalid block number." << endl;
            return false;
        }
        // Assuming each entry in the indirect block is of type int
        int offset = index * sizeof(int);
        if(fseek(sim_disk_fd, (blockNum * block_size) + offset, SEEK_SET) != 0 || fwrite(&value, sizeof(int), 1, sim_disk_fd) != 1) {
            cerr << "Error: Failed to update indirect block." << endl;
            return false;
        }
        return true;
    }
    int GetOrAllocateIndirectFromDouble(int doubleIndirectBlock, int index) {
        if(doubleIndirectBlock < 0 || doubleIndirectBlock >= BitVectorSize) {
            cerr << "Error: Invalid double indirect block number." << endl;
            return -1;
        }

        int offset = index * sizeof(int);
        int singleIndirectBlock;

        if(fseek(sim_disk_fd, (doubleIndirectBlock * block_size) + offset, SEEK_SET) != 0 || fread(&singleIndirectBlock, sizeof(int), 1, sim_disk_fd) != 1) {
            cerr << "Error: Failed to read from double indirect block." << endl;
            return -1;
        }

        // If entry is not set, allocate a new block
        if(singleIndirectBlock == -1) {
            singleIndirectBlock = AllocateBlock();
            if(singleIndirectBlock == -1) {
                cerr << "Error: Unable to allocate block for single indirect." << endl;
                return -1;
            }
            if(!UpdateIndirectBlock(doubleIndirectBlock, index, singleIndirectBlock)) {
                cerr << "Error: Failed to update double indirect block." << endl;
                return -1;
            }
        }

        return singleIndirectBlock;
    }
    // ------------------------------------------------------------------------

    int WriteToFile(int fd, char *buf, int len) {
        cout << "Initial Data: " << string(buf, len) << " with length: " << len << endl;
        if (fd < 0 || fd >= OpenFileDescriptors.size() || !OpenFileDescriptors[fd].isInUse()) {
            cout << "Error: Invalid file descriptor or file not opened." << endl;
            return -1;
        }

        // Get the file's inode
        fsInode* inode = OpenFileDescriptors[fd].getInode();
        if (len > block_size * 3 + block_size * block_size + block_size * block_size * block_size) {
            cout << "Error: Not enough space." << endl;
            return -1;
        }
        // Check if there's enough space - this is a rudimentary check. You should improve this
        if (len > block_size * 3 + block_size * block_size + block_size * block_size * block_size) {
            cout << "Error: Not enough space." << endl;
            return -1;
        }

        int writtenBytes = 0;

        // Write to direct blocks
        for (int i = 0; i < 3 && writtenBytes < len; i++) {
            // Write up to block_size bytes to the direct block
            int bytesToWrite = min(block_size, len - writtenBytes);
            cout << "Direct Block, Bytes to Write: " << bytesToWrite << ", Starting from: " << writtenBytes << endl;
            // Allocate a block and write data to it
            int blockNum = AllocateBlock();
            cout << "Allocated new block: " << blockNum << endl;
            if(blockNum == -1) {
                cerr << "Error: Unable to allocate block for direct write." << endl;
                return writtenBytes; // Return the number of bytes successfully written so far.
            }
            cout << "Attempting to write " << bytesToWrite << " bytes to Block# " << blockNum << endl;
            WriteToBlock(blockNum, buf + writtenBytes, bytesToWrite);
            cout << "Successfully written to Block# " << blockNum << endl;

            // Here, a function like "writeToBlock" would be used to actually write the data
            // For simplicity, let's just update the direct block index
            if (i == 0){
                inode->setDirectBlock1(blockNum);
                cout << "Data written to DirectBlock1: Block# " << blockNum << endl;
            }

            if (i == 1) {
                inode->setDirectBlock2(blockNum);
                cout << "Data written to DirectBlock2: Block# " << blockNum << endl;
            }
            if (i == 2) {
                inode->setDirectBlock3(blockNum);
                cout << "Data written to DirectBlock3: Block# " << blockNum << endl;
            }

            writtenBytes += bytesToWrite;
        }

        // Check if we still have data to write
        if (writtenBytes < len) {
            if (inode->getSingleInDirect() == -1) {
                // Allocate a block for single indirect (this is a simplification)
                inode->setSingleInDirect(AllocateBlock()) ;
            }

            int pointersInBlock = block_size / sizeof(int); // Assuming each pointer is of size 'int'
            for (int i = 0; i < pointersInBlock && writtenBytes < len; i++) {
                // Allocate a data block and write data to it
                int blockNum = AllocateBlock();

                // Write data to this block
                int bytesToWrite = min(block_size, len - writtenBytes);
                cout << "Single Indirect, Bytes to Write: " << bytesToWrite << ", Starting from: " << writtenBytes << endl;
                WriteToBlock(blockNum, buf + writtenBytes, bytesToWrite);
                cout << "Data written to Block#: " << blockNum << " - " << string(buf + writtenBytes, bytesToWrite) << endl;

                // Update the single indirect block to point to this data block
                UpdateIndirectBlock(inode->getSingleInDirect(), i, blockNum);
                cout << "Updated Single Indirect Block: Block# " << blockNum << " at index " << i << endl;

                writtenBytes += bytesToWrite;
            }
        }

        // Check if we still have data to write
        if (writtenBytes < len) {
            if (inode->getDoubleInDirect() == -1) {
                // Allocate a block for double indirect
                inode->setDoubleInDirect(AllocateBlock()) ;
            }

            int pointersInBlock = block_size / sizeof(int);
            for (int i = 0; i < pointersInBlock && writtenBytes < len; i++) {
                // Get or allocate the next single indirect block from the double indirect block
                int indirectBlock = GetOrAllocateIndirectFromDouble(inode->getDoubleInDirect(), i);

                for (int j = 0; j < pointersInBlock && writtenBytes < len; j++) {
                    // Allocate a data block and write data to it
                    int blockNum = AllocateBlock();

                    // Write data to this block
                    int bytesToWrite = min(block_size, len - writtenBytes);
                    cout << "Double Indirect, Bytes to Write: " << bytesToWrite << ", Starting from: " << writtenBytes << endl;
                    WriteToBlock(blockNum, buf + writtenBytes, bytesToWrite);

                    // Update the indirect block to point to this data block
                    UpdateIndirectBlock(indirectBlock, j, blockNum);
                    cout << "Updated Double Indirect Block: Block# " << blockNum << " at index " << j << endl;
                    writtenBytes += bytesToWrite;
                }
            }
        }

        inode->setFileSize(inode->getFileSize()+len);// Update the file size
        inode->setBlockInUse(inode->getBlockInUse()+(int)ceil((double)len/block_size));// Update the number of blocks used
        cout << "File size updated to: " << inode->getFileSize() << " bytes" << endl;
        cout << "Blocks in use: " << inode->getBlockInUse() << endl;

        return writtenBytes;
    }
    // ------------------------------------------------------------------------
    int DelFile( string FileName ) {

    }
    ///////////////////////////////////////////////////////////////////////////////
    static const int MAX_BLOCKS = 1000;
    static const int BLOCK_SIZE = 4096; // or whatever size you've defined for your blocks
    char simulatedDisk[MAX_BLOCKS][BLOCK_SIZE]; // assuming you've defined a max number of blocks somewhere

    char* GetBlockData(int blockNum) {
        if (blockNum < 0 || blockNum >= MAX_BLOCKS) {
            std::cerr << "GetBlockData Error: Block number out of bounds: " << blockNum << std::endl;
            return nullptr;
        }
        std::cout << "Fetching data for block number: " << blockNum << std::endl;
        std::cout << "First few bytes of blockData: ";
        for (int i = 0; (i < 10) && (i < block_size); i++) { // print first 10 bytes or fewer if block is smaller
            std::cout << simulatedDisk[blockNum][i] << " ";
        }
        std::cout << std::endl;
        std::cout << "Memory address of blockData: " << reinterpret_cast<void*>(simulatedDisk[blockNum]) << std::endl;
        return simulatedDisk[blockNum];
    }



    int ReadFromBlock(int blockNum, int startPos, char* buffer, int numBytes, int bufferSize) {
        std::cout << "Reading from block#: " << blockNum << " at position: " << startPos << " for " << numBytes << " bytes." << std::endl;
        if (startPos < 0 || startPos + numBytes > BLOCK_SIZE || numBytes > bufferSize) {
            std::cerr << "ReadFromBlock Error: Invalid start position, bytes to read exceeds block size, or buffer overflow." << std::endl;
            return -1;
        }

        char* blockData = GetBlockData(blockNum);
        if (!blockData) {
            std::cerr << "ReadFromBlock Error: Invalid block number." << std::endl;
            return -1;
        }
        std::cout << "Outside GetBlockData: Memory address of blockData: " << reinterpret_cast<void*>(blockData) << std::endl;
        std::cout << "First few bytes of blockData outside GetBlockData: ";
        for (int i = 0; (i < 10) && (i < block_size); i++) {
            std::cout << blockData[i] << " ";
        }
        std::cout << std::endl;

        for (int i = 0; i < numBytes; i++) {
            buffer[i] = blockData[startPos + i];
            std::cout << "Data is : " << buffer[i] << std::endl;
        }
        buffer[numBytes] = '\0';  // ensure it's null-terminated for print
        std::cout << "Data read from block: " << buffer << std::endl;
        return numBytes;  // return the number of bytes actually read
    }



    // ------------------------------------------------------------------------
    int ReadFromFile(int fd, char* buf, int count) {
        std::cout << "Attempting to read from file with descriptor: " << fd << std::endl;
        const int max_open_files = 100;
        if (fd < 0 || fd >= max_open_files || !OpenFileDescriptors[fd].isInUse()) {
            return -1;  // error: invalid file descriptor
        }

        fsInode* inode = OpenFileDescriptors[fd].getInodePtr();
        int currentPosition = OpenFileDescriptors[fd].getCurrentPosition();
        if (!inode) {
            std::cerr << "Error: inode pointer is null." << std::endl;
            return -1;  // or return an appropriate error value if the function expects a return
        }
        std::cout << "File SIze :  " << inode->getFileSize() << std::endl;

        int remainingBytes = count;
        int bytesRead = 0;
        int blockNum = -1;  // Initialize the blockNum

        while (remainingBytes > 0 && currentPosition < inode->getFileSize()) {
            int blockOffset = currentPosition / block_size;
            int offsetWithinBlock = currentPosition % block_size;

            blockNum = inode->getBlockNumberForOffset(blockOffset);

            int bytesToRead = min(block_size - offsetWithinBlock, remainingBytes);
            bytesToRead = min(bytesToRead, inode->getFileSize() - currentPosition);

            std::cout << "Attempting to read " << bytesToRead << " bytes from Block# " << blockNum << " starting at position " << offsetWithinBlock << std::endl;

            char tempBuffer[BLOCK_SIZE + 1] = {};  // creating a temporary buffer and initializing it with null bytes
            if (blockNum < 0 || blockNum >= MAX_BLOCKS) {
                std::cerr << "Error: Block number out of bounds: " << blockNum << std::endl;
                break;  // exit the reading loop
            }

            int bytesActuallyRead = ReadFromBlock(blockNum, offsetWithinBlock, tempBuffer, bytesToRead, BLOCK_SIZE);
            if (bytesActuallyRead == -1) {
                std::cerr << "Error reading from block" << std::endl;
                break;  // exit the loop if there's an error
            }

            // Copy the data from tempBuffer to the main buffer (buf)
            for (int i = 0; i < bytesActuallyRead; i++) {
                buf[bytesRead + i] = tempBuffer[i];
            }

            bytesRead += bytesActuallyRead;
            currentPosition += bytesActuallyRead;
            remainingBytes -= bytesActuallyRead;

            // Update the current position for the file descriptor (if you're keeping track of it)
            OpenFileDescriptors[fd].setCurrentPosition(currentPosition);
        }

        // Null terminate the buffer after reading. Ensure buffer has space for the null terminator.
        buf[bytesRead] = '\0';

        return bytesRead;  // Return the total number of bytes read.
    }



    // ------------------------------------------------------------------------
    int GetFileSize(int fd) {

    }

    // ------------------------------------------------------------------------
    int CopyFile(string srcFileName, string destFileName) {}

    // ------------------------------------------------------------------------
    int MoveFile(string srcFileName, string destFileName) {}

    // ------------------------------------------------------------------------
    int RenameFile(string oldFileName, string newFileName) {}

};

int main() {
    int blockSize;
    int direct_entries;
    string fileName;
    string fileName2;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read;
    int _fd;

    fsDisk *fs = new fsDisk();
    int cmd_;
    while(1) {
        cin >> cmd_;
        switch (cmd_)
        {
            case 0:   // exit
                delete fs;
                exit(0);
                break;

            case 1:  // list-file
                fs->listAll();
                break;

            case 2:    // format
                cin >> blockSize;
                cin >> direct_entries;
                fs->fsFormat(blockSize, direct_entries);
                break;

            case 3:    // creat-file
                cin >> fileName;
                _fd = fs->CreateFile(fileName);
                cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 4:  // open-file
                cin >> fileName;
                _fd = fs->OpenFile(fileName);
                cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 5:  // close-file
                cin >> _fd;
                fileName = fs->CloseFile(_fd);
                cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 6:   // write-file
                cin >> _fd;
                cin >> str_to_write;
                fs->WriteToFile( _fd , str_to_write , strlen(str_to_write) );
                break;

            case 7:    // read-file
                cin >> _fd;
                cin >> size_to_read ;
                fs->ReadFromFile( _fd , str_to_read , size_to_read );
                cout << "ReadFromFile:     " << str_to_read << endl;
                break;

            case 8:   // delete file
                cin >> fileName;
                _fd = fs->DelFile(fileName);
                cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 9:   // copy file
                cin >> fileName;
                cin >> fileName2;
                fs->CopyFile(fileName, fileName2);
                break;

            case 10:  // rename file
                cin >> fileName;
                cin >> fileName2;
                fs->RenameFile(fileName, fileName2);
                break;

            default:
                break;
        }
    }

}
