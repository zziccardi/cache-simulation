
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;


int lineCount = 0;

// ***** Part 1: direct mapped ***** //
int dmHitCount1  = 0;
int dmHitCount4  = 0;
int dmHitCount16 = 0;
int dmHitCount32 = 0;

// ***** Part 2: set associative ***** //
int saHitCount2  = 0;
int saHitCount4  = 0;
int saHitCount8  = 0;
int saHitCount16 = 0;

// ***** Part 3: fully associative ***** //
int faLruHitCount = 0;
int faHcHitCount  = 0;

class Node {
    public:
        Node* parent;
        Node* left;
        Node* right;
        bool  goLeft;
        int   value;
        
        Node() {
            parent = NULL;
            left   = NULL;
            right  = NULL;
            goLeft = true;
            value  = -1;
        }
};

// Binary tree
// Only the nodes in the bottom row should have a value
Node* head = NULL;

// Holds pointers to the nodes in the bottom row of the tree for convenient access
vector<Node*> bottomRow;

// ***** Part 4: set associative with no allocation on write miss ***** //
int noAllocHitCount2  = 0;
int noAllocHitCount4  = 0;
int noAllocHitCount8  = 0;
int noAllocHitCount16 = 0;

// ***** Part 5: set associative with next-line prefetching ***** //
int prefetchHitCount2  = 0;
int prefetchHitCount4  = 0;
int prefetchHitCount8  = 0;
int prefetchHitCount16 = 0;

// ***** Part 6: set associative with next-line prefetching, but only on a cache miss ***** //
int prefetchOnMissHitCount2  = 0;
int prefetchOnMissHitCount4  = 0;
int prefetchOnMissHitCount8  = 0;
int prefetchOnMissHitCount16 = 0;


// Used to create the tree needed for hot-cold LRU approximation
Node* createSubtree(const int& depth, Node* parent) {
    Node* node = new Node();
    
    node->parent = parent;
    
    if (depth == 1) {
        bottomRow.push_back(node);
        
        return node;
    }
    
    node->left  = createSubtree(depth - 1, node);
    node->right = createSubtree(depth - 1, node);
    
    return node;
}

// Also known as pseudo-LRU
void fullyAssociativeHotCold(const string& address) {
    string tag = "";
    
    // Because there is only one set, there is no index; the tag includes all but the address' offset bits
    for (int i = 0; i < 32 - 5; i++) {
        tag += address[i];
    }
    
    int tagInt = stoi(tag, NULL, 2);
    
    bool found = false;
    
    for (int i = 0; i < bottomRow.size(); i++) {
        // If the tag is already in the set, it's a cache hit
        if (bottomRow[i]->value == tagInt) {
            faHcHitCount++;
            
            Node* cur  = bottomRow[i];
            Node* temp = bottomRow[i]->parent;
            
            // Flip the parent's hot-cold bit so it points the other way next time
            while (temp != NULL) {
                if (temp->left == cur && temp->goLeft == true) {
                    temp->goLeft = false;
                }
                else if (temp->right == cur && temp->goLeft == false) {
                    temp->goLeft = true;
                }
                
                cur  = temp;
                temp = temp->parent;
            }
            
            found = true;
            break;
        }
    }
    
    // If the tag isn't already in the set, it's a cache miss, so add it
    if (!found) {
        Node* cur     = head;
        Node* next    = NULL;
        Node* curCopy = NULL;
        
        // Navigate down the tree according to cur->goLeft and switch its value afterward
        while (cur != NULL) {
            // Copy the node pointer so I can access it after the loop (at which point cur is null)
            curCopy     = cur;
            next        = (cur->goLeft) ? cur->left : cur->right;
            cur->goLeft = (cur->goLeft) ? false     : true;
            
            cur = next;
        }
        
        // curCopy is the selected node, so replace any preexisting value with the new one
        curCopy->value = tagInt;
    }
}

void fullyAssociativeLru(const string& address) {
    static vector<int> cache;
    
    int numOfWays = 512;
    
    string tag = "";
    
    // Because there is only one set, there is no index; the tag includes all but the address' offset bits
    for (int i = 0; i < 32 - 5; i++) {
        tag += address[i];
    }
    
    int tagInt = stoi(tag, NULL, 2);
    
    bool found = false;
    
    for (int i = 0; i < cache.size(); i++) {
        // If the tag is already in the set, it's a cache hit
        if (cache[i] == tagInt) {
            faLruHitCount++;
            
            // Nothing needs to be done if the element containing the tag is already the most recently used element
            if (i != 0) {
                // Remove the element that contains the tag already
                cache.erase(cache.begin() + i);
                
                cache.insert(cache.begin(), tagInt);
            }
            
            found = true;
            break;
        }
    }
    
    // If the tag isn't already in the set, it's a cache miss, so add it
    if (!found) {        
        // ex: If there are 4 ways and the vector already contains 4 elements, carry out the LRU replacement process
        if (cache.size() == numOfWays) {
            // Remove the least recently used element, which is at the end
            cache.erase(cache.end() - 1);
            
            cache.insert(cache.begin(), tagInt);
        }
        // If the vector hasn't been "filled" yet, just insert it to the front (it becomes the most recently used element)
        else {
            cache.insert(cache.begin(), tagInt);
        }
    }
}

void setAssociative(
    const int&    numOfWays,
    const string& address,
    const bool&   noAlloc,
    const string& lsFlag,
    const bool&   alwaysPrefetch,
    const bool&   prefetchOnMiss) {
    // Cache size is always 16 KB (2^14 = 16384)
    // Number of entries = cache size / cache line
    // Thus, 16384 / 32 = 512 entries
    //
    //  2-way: 512 / 2  = 256 sets
    //  4-way: 512 / 4  = 128
    //  8-way: 512 / 8  =  64
    // 16-way: 512 / 16 =  32
    
    // Create an array of vectors, which represent the sets
    // Each vector is used for implementing the LRU replacement policy for the ways in the set
    static vector<int> sa2[256];
    static vector<int> sa4[128];
    static vector<int> sa8[64];
    static vector<int> sa16[32];
    
    static vector<int> noAlloc2[256];
    static vector<int> noAlloc4[128];
    static vector<int> noAlloc8[64];
    static vector<int> noAlloc16[32];
    
    static vector<int> prefetch2[256];
    static vector<int> prefetch4[128];
    static vector<int> prefetch8[64];
    static vector<int> prefetch16[32];
    
    static vector<int> onMiss2[256];
    static vector<int> onMiss4[128];
    static vector<int> onMiss8[64];
    static vector<int> onMiss16[32];
    
    vector<int>* cache         = NULL;
    int*         hitCount      = NULL;
    int          indexBitCount = 0;
    
    // Regular set-associative
    if (!noAlloc && !alwaysPrefetch && !prefetchOnMiss) {
        switch (numOfWays) {
            case 2:
                cache         = sa2;
                hitCount      = &saHitCount2;
                indexBitCount = 8;
                break;
            case 4:
                cache         = sa4;
                hitCount      = &saHitCount4;
                indexBitCount = 7;
                break;
            case 8:
                cache         = sa8;
                hitCount      = &saHitCount8;
                indexBitCount = 6;
                break;
            case 16:
                cache         = sa16;
                hitCount      = &saHitCount16;
                indexBitCount = 5;
                break;
            default:
                fprintf(stderr, "Invalid number of ways passed to setAssociative\n");
                exit(EXIT_FAILURE);
        }
    }
    // No allocation on write miss
    else if (noAlloc) {
        switch (numOfWays) {
            case 2:
                cache         = noAlloc2;
                hitCount      = &noAllocHitCount2;
                indexBitCount = 8;
                break;
            case 4:
                cache         = noAlloc4;
                hitCount      = &noAllocHitCount4;
                indexBitCount = 7;
                break;
            case 8:
                cache         = noAlloc8;
                hitCount      = &noAllocHitCount8;
                indexBitCount = 6;
                break;
            case 16:
                cache         = noAlloc16;
                hitCount      = &noAllocHitCount16;
                indexBitCount = 5;
                break;
            default:
                fprintf(stderr, "Invalid number of ways passed to setAssociative\n");
                exit(EXIT_FAILURE);
        }
    }
    // Next-line prefetching
    else if (alwaysPrefetch) {
        switch (numOfWays) {
            case 2:
                cache         = prefetch2;
                hitCount      = &prefetchHitCount2;
                indexBitCount = 8;
                break;
            case 4:
                cache         = prefetch4;
                hitCount      = &prefetchHitCount4;
                indexBitCount = 7;
                break;
            case 8:
                cache         = prefetch8;
                hitCount      = &prefetchHitCount8;
                indexBitCount = 6;
                break;
            case 16:
                cache         = prefetch16;
                hitCount      = &prefetchHitCount16;
                indexBitCount = 5;
                break;
            default:
                fprintf(stderr, "Invalid number of ways passed to setAssociative\n");
                exit(EXIT_FAILURE);
        }
    }
    else if (prefetchOnMiss) {
        switch (numOfWays) {
            case 2:
                cache         = onMiss2;
                hitCount      = &prefetchOnMissHitCount2;
                indexBitCount = 8;
                break;
            case 4:
                cache         = onMiss4;
                hitCount      = &prefetchOnMissHitCount4;
                indexBitCount = 7;
                break;
            case 8:
                cache         = onMiss8;
                hitCount      = &prefetchOnMissHitCount8;
                indexBitCount = 6;
                break;
            case 16:
                cache         = onMiss16;
                hitCount      = &prefetchOnMissHitCount16;
                indexBitCount = 5;
                break;
            default:
                fprintf(stderr, "Invalid number of ways passed to setAssociative\n");
                exit(EXIT_FAILURE);
        }
    }
    
    string tag   = "";
    string index = "";
    
    // The address is always 32 bits, with the offset always being the 5 least significant bits
    // (The offset bits are completely ignored)
    for (int i = 0; i < 32 - 5 - indexBitCount; i++) {
        tag += address[i];
    }
    
    for (int i = tag.size(); i < tag.size() + indexBitCount; i++) {
        index += address[i];
    }
    
    // Convert the index bit string to an actual integral value so it can be used to index the cache
    int indexInt = stoi(index, NULL, 2);
    
    // Convert the tag to an int for more efficient storage in an array (the cache)
    int tagInt = stoi(tag, NULL, 2);
    
    vector<int>* whichSet = &(cache[indexInt]);
    
    bool found = false;
    
    for (int i = 0; i < whichSet->size(); i++) {
        // If the tag is already in the set, it's a cache hit
        if (whichSet->at(i) == tagInt) {
            (*hitCount)++;
            
            // Nothing needs to be done if the element containing the tag is already the most recently used element
            if (i != 0) {
                // Remove the element that contains the tag already
                whichSet->erase(whichSet->begin() + i);
                
                whichSet->insert(whichSet->begin(), tagInt);
            }
            
            found = true;
            break;
        }
    }
    
    // If the tag isn't already in the set, it's a cache miss, so add it
    if (!found) {
        // If this function is being called for a cache with no allocation on write miss and it's a store instruction,
        // it's not added to the cache
        if (noAlloc && lsFlag == "S") {
            return;
        }
        
        // ex: If there are 4 ways and the vector already contains 4 elements, carry out the LRU replacement process
        if (whichSet->size() == numOfWays) {
            // Remove the least recently used element, which is at the end
            whichSet->erase(whichSet->end() - 1);
            
            whichSet->insert(whichSet->begin(), tagInt);
        }
        // If the vector hasn't been "filled" yet, just insert it to the front (it becomes the most recently used element)
        else {
            whichSet->insert(whichSet->begin(), tagInt);
        }
    }
    
    if (alwaysPrefetch || (prefetchOnMiss && !found)) {
        // If the index overflows when incrementing it to bring the next cache line into the cache,
        // set the index back to zero and increment the tag (the LSB of the tag is essentially a carry bit)
        //
        // For example, if numOfWays == 2, then indexBitCount == 8
        // Therefore, 2^8 == 256
        // This means 255 is the max value indexInt can be before overflowing
        if (indexInt == pow(2, indexBitCount) - 1) {
            whichSet = &(cache[0]);
            
            tagInt++;
        }
        // The index won't overflow, so it's fine to just increment it
        else {
            whichSet = &(cache[indexInt + 1]);
        }
        
        found = false;
        
        for (int i = 0; i < whichSet->size(); i++) {
            // If the tag is already in the set, the next line is already in the cache
            // NOTE: Do not increment the hit count here because this is not an actual access
            if (whichSet->at(i) == tagInt) {
                // Nothing needs to be done if the element containing the tag is already the most recently used element
                if (i != 0) {
                    // Remove the element that contains the tag already
                    whichSet->erase(whichSet->begin() + i);
                    
                    whichSet->insert(whichSet->begin(), tagInt);
                }
                
                found = true;
                break;
            }
        }
        
        // If the tag isn't already in the set, the next line isn't already in the cache, so add it
        if (!found) {
            // ex: If there are 4 ways and the vector already contains 4 elements, carry out the LRU replacement process
            if (whichSet->size() == numOfWays) {
                // Remove the least recently used element, which is at the end
                whichSet->erase(whichSet->end() - 1);
                
                whichSet->insert(whichSet->begin(), tagInt);
            }
            // If the vector hasn't been "filled" yet, just insert it to the front (it becomes the most recently used element)
            else {
                whichSet->insert(whichSet->begin(), tagInt);
            }
        }
    }
}

void directMapped(const int& cacheSize, const string& address) {
    // Cache size of 1 KB means 2^10 (1024)
    // Number of entries = cache size / cache line
    // ex: 1024 / 32 = 32
    static int dm1[32]    = {0};
    static int dm4[128]   = {0};
    static int dm16[512]  = {0};
    static int dm32[1024] = {0};
    
    int* cache         = NULL;
    int* hitCount      = NULL;
    int  indexBitCount = 0;
    
    switch (cacheSize) {
        case 1:
            cache         = dm1;
            hitCount      = &dmHitCount1;
            indexBitCount = 5;
            break;
        case 4:
            cache         = dm4;
            hitCount      = &dmHitCount4;
            indexBitCount = 7;
            break;
        case 16:
            cache         = dm16;
            hitCount      = &dmHitCount16;
            indexBitCount = 9;
            break;
        case 32:
            cache         = dm32;
            hitCount      = &dmHitCount32;
            indexBitCount = 10;
            break;
        default:
            fprintf(stderr, "Invalid cache size passed to directMapped\n");
            exit(EXIT_FAILURE);
    }
    
    string tag   = "";
    string index = "";
    
    // The address is always 32 bits, with the offset always being the 5 least significant bits
    // (The offset bits are completely ignored)
    for (int i = 0; i < 32 - 5 - indexBitCount; i++) {
        tag += address[i];
    }
    
    for (int i = tag.size(); i < tag.size() + indexBitCount; i++) {
        index += address[i];
    }
    
    /*
    cout << "Address: " << address << endl;
    cout << "Tag:     " << tag     << " (size: " << tag.size()   << ")" << endl;
    cout << "Index:   " << index   << " (size: " << index.size() << ")" << endl;
    */
    
    // Convert the index bit string to an actual integral value so it can be used to index the cache
    int indexInt = stoi(index, NULL, 2);
    
    // Convert the tag to an int for more efficient storage in an array (the cache)
    int tagInt = stoi(tag, NULL, 2);
    
    //cout << "Index: " << indexInt << endl;
    //cout << "Tag:   " << tagInt   << endl;
    
    // If the tag is already stored in the cache at this index, it's a cache hit
    if (cache[indexInt] == tagInt) {
        (*hitCount)++;
    }
    else {
        cache[indexInt] = tagInt;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "This program requires two command-line arguments.\n");
        exit(EXIT_FAILURE);
    }
    
    ifstream inputFile(argv[1]);
    
    if (inputFile.is_open()) {
        // Either "L" (load) or "S" (store)
        string             lsFlag;
        unsigned long long address;
        string             line;
        
        // Create the tree needed for the fully associative cache's hot-cold LRU approximation
        head = createSubtree(10, NULL);
        
        //cout << "Number of nodes in bottom row: " << bottomRow.size() << endl;
        
        while (getline(inputFile, line)) {
            stringstream ss(line);
            
            ss >> lsFlag;
            
            // Read in hexadecimal address and store its decimal representation in `address`
            ss >> std::hex >> address;
            
            lineCount++;
            
            // Convert the address to its bit representation
            bitset<32> b(address);
            
            string addressBits = b.to_string();
            
            directMapped(1, addressBits);
            directMapped(4, addressBits);
            directMapped(16, addressBits);
            directMapped(32, addressBits);
            
            setAssociative(2, addressBits, false, lsFlag, false, false);
            setAssociative(4, addressBits, false, lsFlag, false, false);
            setAssociative(8, addressBits, false, lsFlag, false, false);
            setAssociative(16, addressBits, false, lsFlag, false, false);
            
            fullyAssociativeLru(addressBits);
            fullyAssociativeHotCold(addressBits);
            
            setAssociative(2, addressBits, true, lsFlag, false, false);
            setAssociative(4, addressBits, true, lsFlag, false, false);
            setAssociative(8, addressBits, true, lsFlag, false, false);
            setAssociative(16, addressBits, true, lsFlag, false, false);
            
            setAssociative(2, addressBits, false, lsFlag, true, false);
            setAssociative(4, addressBits, false, lsFlag, true, false);
            setAssociative(8, addressBits, false, lsFlag, true, false);
            setAssociative(16, addressBits, false, lsFlag, true, false);
            
            setAssociative(2, addressBits, false, lsFlag, false, true);
            setAssociative(4, addressBits, false, lsFlag, false, true);
            setAssociative(8, addressBits, false, lsFlag, false, true);
            setAssociative(16, addressBits, false, lsFlag, false, true);
        }
        
        inputFile.close();
    }
    else {
        fprintf(stderr, "Error opening input file\n");
        exit(EXIT_FAILURE);
    }
    
    ofstream outputFile(argv[2]);
    
    if (outputFile.is_open()) {
        outputFile << dmHitCount1  << "," << lineCount << "; ";
        outputFile << dmHitCount4  << "," << lineCount << "; ";
        outputFile << dmHitCount16 << "," << lineCount << "; ";
        outputFile << dmHitCount32 << "," << lineCount << ";" << endl;
        
        outputFile << saHitCount2  << "," << lineCount << "; ";
        outputFile << saHitCount4  << "," << lineCount << "; ";
        outputFile << saHitCount8  << "," << lineCount << "; ";
        outputFile << saHitCount16 << "," << lineCount << ";" << endl;
        
        outputFile << faLruHitCount << "," << lineCount << ";" << endl;
        
        outputFile << faHcHitCount << "," << lineCount << ";" << endl;
        
        outputFile << noAllocHitCount2  << "," << lineCount << "; ";
        outputFile << noAllocHitCount4  << "," << lineCount << "; ";
        outputFile << noAllocHitCount8  << "," << lineCount << "; ";
        outputFile << noAllocHitCount16 << "," << lineCount << ";" << endl;
        
        outputFile << prefetchHitCount2  << "," << lineCount << "; ";
        outputFile << prefetchHitCount4  << "," << lineCount << "; ";
        outputFile << prefetchHitCount8  << "," << lineCount << "; ";
        outputFile << prefetchHitCount16 << "," << lineCount << ";" << endl;
        
        outputFile << prefetchOnMissHitCount2  << "," << lineCount << "; ";
        outputFile << prefetchOnMissHitCount4  << "," << lineCount << "; ";
        outputFile << prefetchOnMissHitCount8  << "," << lineCount << "; ";
        outputFile << prefetchOnMissHitCount16 << "," << lineCount << ";" << endl;
        
        outputFile.close();
    }
    else {
        fprintf(stderr, "Error opening output file\n");
        exit(EXIT_FAILURE);
    }
}

