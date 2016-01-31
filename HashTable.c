#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HashTable.h"

struct HashTableObjectTag
{
        int sentinel;
    int dynamicBehaviour;
    float useFactor;
    float contractUseFactor;
    float expandUseFactor;
    unsigned char Size;
        struct node **array;
};

struct node
{
    void* data;
    char* key;
    struct node *left;
    struct node *right;
};

static struct node *NewNode(void* data, char* key);
unsigned char IntHash (int key, unsigned char Size);
unsigned char StringHash (char *key, unsigned char Size);
int InsertHelper(struct node** root, int status, char *key, void *data, void** previousDataHandle);
int DeleteHelper (struct node **root, char *key, void **dataHandle);
void GetKeysHelper(struct node** root, char **temp_array, int *index);
int FindHelper (HashTablePTR hashTable, struct node** root, char *key, void **dataHandle);
int largestBucket(HashTablePTR hashTable, struct node **root, int count, int *largest);
int Resize(HashTablePTR hashTable); 
int ResizeHelper(HashTablePTR hashTable, unsigned char size, void **dataArray, char **keysArray);

static int keycount = 0;        // keep track of number of keys
static int temp_keycount;        // keep track of number of keys during resizing
static int preserve = 0;        // destroy entire thing (0), just trees & preserve table (1)


// Function that creates a new node
static struct node *NewNode(void* data, char* key)
{
    struct node *newnode = malloc(sizeof(struct node));
    newnode -> key = malloc((strlen(key) + 1)*sizeof(char));
    strcpy(newnode->key,key);
    newnode -> data = data;
    newnode -> left = NULL;
    newnode -> right = NULL;

    return newnode;
}

unsigned char IntHash (int key, unsigned char Size)
{
    if (Size == 0)
    {    return 0;}

    int result;
    result = (key % Size);
    if (result < 0)
    {
        result = -result;
    }
    return (unsigned char)result;
}

unsigned char StringHash (char *key, unsigned char Size)
{
    int total = 0;
    for (int i = 0; i < strlen (key); i++)
    {
        total = total + (int)key[i];
    }
    return IntHash(total, Size); 
}


int CreateHashTable (HashTablePTR *hashTableHandle, unsigned int initialSize)
{
    HashTableObject *temp;    //set temp to Handle as a test for proper memory allocation
    temp = malloc(sizeof(HashTableObject));
    if (temp == NULL)
    {    return -1;}
    
    struct node **new_array = calloc(initialSize, sizeof(struct node));
    if (new_array == NULL)
    {    return -1;} 

    HashTableInfo *pHashTableInfo = malloc(sizeof(HashTableInfo));
    if (pHashTableInfo == NULL)
    {    return -1;}

    (*temp).Size = (unsigned char) initialSize;
    
    if (initialSize == 0)            
    {    temp -> array = NULL;}       

    for (int i = 0; i < initialSize; i++)
    {    new_array[i] = NULL;}

    temp -> array = new_array;
    *hashTableHandle = temp;    //set PTR to point back to array
    (*hashTableHandle)-> sentinel = (int)0xDEADBEEF;

    GetHashTableInfo(*hashTableHandle, pHashTableInfo);
    (*hashTableHandle) -> dynamicBehaviour = 1;
    (*hashTableHandle) -> contractUseFactor = (float)0.2;
    (*hashTableHandle) -> expandUseFactor = (float)0.7;
    free(pHashTableInfo);
    
    return 0;
}



int DestroyHashTable (HashTablePTR *hashTableHandle)
{
    if ((*hashTableHandle) == NULL)
    {    return -1;}

    if (*((int*)*hashTableHandle) != 0xDEADBEEF)
    {    return -1;}

    if ((*hashTableHandle) -> Size == 0)            // if the size of hashtable is zero
    {    free(((*hashTableHandle) -> array));        // free the array (it is empty)
        free(*hashTableHandle);                // free the hashTableobject
        *hashTableHandle = NULL;            // set the handle to point to NULL
        return 0;}
   

    struct node **head;
    (*hashTableHandle)->dynamicBehaviour = 0;
    for (int i = 0; i < (**hashTableHandle).Size; i++)      // goes through indices
    {
        void *dataHandle = NULL;
        head = &((*hashTableHandle) -> array[i]);        
        while (*head != NULL)
        {    head = &((*hashTableHandle) -> array[i]);    
            DeleteEntry (*hashTableHandle, (*head)->key, &dataHandle);}
    }
    
    free(((*hashTableHandle) -> array));
    if (preserve == 0)
    {
        free(*hashTableHandle);
        *hashTableHandle = NULL;
    }

    return 0;
}



int InsertHelper(struct node** root, int status, char *key, void *data, void** previousDataHandle)
{
    if (*root == NULL)
    {    *root = NewNode(data, key);
        keycount++;
        return status;
    }

    // if the key already exists in the tree, replace its data with the new one
    if (strcmp(key,((*root)->key)) == 0)
    {    *previousDataHandle = ((*root)->data);
        (*root)->data = data;
        return 2;}

    // otherwise iterate through branches with recursion (if hash is already occupied)
    else if (strcmp(key, (*root)->key) < 0)
    {    return InsertHelper(&(*root)->left, 1, key, data, previousDataHandle);}

    else if (strcmp(key, (*root)->key) > 0)
    {    return InsertHelper(&(*root)->right, 1, key, data, previousDataHandle);}

    return -2;
}



int InsertEntry (HashTablePTR hashTable, char *key, void *data, void **previousDataHandle)
{
    if (hashTable == NULL) 
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    if (hashTable -> Size == 0)
    {    return -2;}

    HashTableInfo *pHashTableInfo = malloc(sizeof(HashTableInfo));
    if (pHashTableInfo == NULL)
    {    return -1;}

    int status = 0;
    unsigned char hash = StringHash(key, hashTable -> Size);
    status = InsertHelper(&(hashTable->array[hash]), status, key, data, previousDataHandle);
    
    GetHashTableInfo(hashTable, pHashTableInfo);
    hashTable->useFactor = pHashTableInfo->useFactor;
    hashTable->expandUseFactor = pHashTableInfo->expandUseFactor;

    if ((hashTable->dynamicBehaviour!= 0) && ((hashTable->useFactor>=hashTable->expandUseFactor) || (hashTable->useFactor<=hashTable->contractUseFactor)))
    {    Resize(hashTable);}

    free(pHashTableInfo);
    return status;
}



int DeleteHelper (struct node **root, char *key, void **dataHandle)
{
    if (*root != NULL)
    {    
        if (strcmp(key, (*root)->key) < 0)    // looking for the point
        {    return DeleteHelper(&((*root)->left), key, dataHandle);}

        else if (strcmp(key, (*root)->key) > 0)
        {    return DeleteHelper(&((*root)->right), key, dataHandle);}

        else             // if strcmp(key, (hashTable[hash])->key) == 0)
        {
            struct node* temp = *root;
            
            if (((*root)->left == NULL) && ((*root)->right == NULL))
            {    *dataHandle = (*root)->data;
                free(temp->key);
                free(temp);
                *root = NULL;
                keycount--;}

            else if ((*root)->left == NULL)    //&& ((*root)->right != NULL))
            {    *dataHandle = (*root)->data;
                *root = (*root)->right;
                free(temp -> key);
                free(temp);
                keycount--;}

            else if ((*root)->right == NULL)   //&& ((*root)->left != NULL))
            {    *dataHandle = (*root)->data;
                *root = (*root)->left;
                free(temp->key);
                free(temp);
                keycount--;}

            else     //if (((*root)->left != NULL) && ((*root)->right != NULL))
            {    
                struct node* bottomleft = (*root)->right;
                struct node* follows = *root;
                while (bottomleft->left != NULL)
                {    follows = bottomleft;    
                    bottomleft = bottomleft->left;}

                bottomleft->left = ((*root)->left);
                *dataHandle = (*root)->data;
                *root = (*root)->right;
                free(temp->key);            
                free(temp);
                keycount--;
            }
            return 0;
        }
    }
    return -2;
}



int DeleteEntry (HashTablePTR hashTable, char *key, void **dataHandle)
{
    if (hashTable == NULL) 
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    if (hashTable -> Size == 0)
    {    return -2;}

    HashTableInfo *pHashTableInfo = malloc(sizeof(HashTableInfo));
    if (pHashTableInfo == NULL)
    {    return -1;}


    // The number of buckets in the HashTable cannot be less than 1
    if ((keycount == 1) && (hashTable->dynamicBehaviour != 0))
    {    hashTable->dynamicBehaviour = 0;}


    unsigned char hash = StringHash(key, hashTable -> Size);
    int status = DeleteHelper(&(hashTable->array[hash]), key, dataHandle);

    GetHashTableInfo(hashTable, pHashTableInfo);
    hashTable->useFactor = pHashTableInfo->useFactor;
    hashTable->expandUseFactor = pHashTableInfo->expandUseFactor;


    if ((hashTable->dynamicBehaviour!= 0) && ((hashTable->useFactor>=hashTable->expandUseFactor) || (hashTable->useFactor<=hashTable->contractUseFactor)))
    {    Resize(hashTable);}

    free(pHashTableInfo);
    return status;
}



int FindHelper (HashTablePTR hashTable, struct node** root, char *key, void **dataHandle)
{
    if (*root == NULL)                 // item not in the tree 
    {    *dataHandle = NULL;
        return -2;}

    else if (strcmp(key, (*root)->key) == 0)     // item has been found
    {    *dataHandle = (*root)->data;
        return 0;
    }

    else if (strcmp(key, (*root)->key) < 0)        // recurse using left child branch
    {    return FindHelper(hashTable, &((*root) -> left), key, dataHandle);}    

    else if (strcmp(key, (*root)->key) > 0)     // recurse using right child branch
    {    return FindHelper(hashTable, &((*root) -> right), key, dataHandle);}

    *dataHandle = NULL;
    return -2;
}

int FindEntry (HashTablePTR hashTable, char *key, void **dataHandle)
{
    if (hashTable == NULL) 
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    if (hashTable -> Size == 0)
    {    return -2;}

    unsigned char hash = StringHash(key, hashTable -> Size);
    return FindHelper(hashTable, &(hashTable->array[hash]), key, dataHandle);
}



void GetKeysHelper(struct node** root, char **temp_array, int *index)
{
    if (*root != NULL)
    {    
        unsigned long size = strlen((*root)->key ) + 1;
        temp_array[*index] = malloc((size)* sizeof(char*));
        temp_array[*index] = strcpy(temp_array[*index] , (*root)->key);
        *index += 1;
        GetKeysHelper(&((*root)->left), temp_array, index);
        GetKeysHelper(&((*root)->right), temp_array, index);
    }
}



int GetKeys(HashTablePTR hashTable, char ***keysArrayHandle, unsigned int *keyCount)
{
    if (hashTable == NULL)
    {    *keyCount = 0;
        return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    if (hashTable -> Size == 0)
    {    return 0;}
    
    *keyCount = (unsigned int)keycount;     
    int index = 0;
    char** temp_array = calloc((unsigned long)(*keyCount),sizeof(char*));
    for (int i = 0; i < hashTable->Size; i++)
    {    GetKeysHelper(&(hashTable->array[i]), temp_array, &index);} 
    
    if (temp_array == NULL)        // if it couldnt allocate memory properly
    {    return -2;}

    *keysArrayHandle = temp_array;
    return 0;
}    



int GetHashTableInfo (HashTablePTR hashTable, HashTableInfo *pHashTableInfo)
{
    if (hashTable == NULL)
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    pHashTableInfo -> bucketCount = hashTable->Size;
    float keycounter = (float)keycount;
    float bucketcounter = (float)(hashTable -> Size);    // will change with resizing
    pHashTableInfo->loadFactor = (float)(keycounter/bucketcounter);
    
    int nonemptycount = 0;
    for (int i = 0; i < hashTable->Size; i++)
    {
        if ((hashTable->array[i] != NULL) || (hashTable->array[i] != NULL))
        {    nonemptycount++;}
    }
    pHashTableInfo->useFactor = ((float)nonemptycount)/((float)(hashTable->Size));
    hashTable->useFactor = pHashTableInfo->useFactor;

    int current, largest = 0;
    int temp = 0;
    int *bigcount = &temp;
    for (int i = 0; i < bucketcounter; i++)
    {    
        current = largestBucket(hashTable, &(hashTable->array[i]), 0, bigcount);
        if (current > largest)
        {    largest = current;}    
    }
    pHashTableInfo -> largestBucketSize = (unsigned int)largest;

    pHashTableInfo -> dynamicBehaviour = hashTable -> dynamicBehaviour;
    pHashTableInfo -> expandUseFactor = hashTable -> expandUseFactor;
    pHashTableInfo -> contractUseFactor = hashTable -> contractUseFactor;

    return 0;
}



int largestBucket(HashTablePTR hashTable, struct node **root, int count, int* largest)
{

    if ((*root) == NULL)
    {    return *largest;}

    count++;
    if (count > *largest)
    {    *largest = count;}

    largestBucket(hashTable, &((*root)->left), count, largest);
    largestBucket(hashTable, &((*root)->right), count, largest);

    return *largest;
}



int SetResizeBehaviour (HashTablePTR hashTable, int dynamicBehaviour, float expandUseFactor, float contractUseFactor)
{
    if (hashTable == NULL)
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}

    if ((expandUseFactor > 1) || (contractUseFactor < 0))
    {    return 1;}    

    hashTable -> dynamicBehaviour = dynamicBehaviour;
    hashTable -> expandUseFactor = expandUseFactor;
    hashTable -> contractUseFactor = contractUseFactor;

    if (expandUseFactor <= contractUseFactor)
    {    return 1;}

    if ((hashTable -> dynamicBehaviour != 0) && ((hashTable->useFactor >=expandUseFactor) || (hashTable->useFactor <= contractUseFactor)))
    {    Resize(hashTable);}

    return 0;
}



int Resize(HashTablePTR hashTable)
{
    if (hashTable == NULL) 
    {    return -1;}

    if (*((int*)hashTable) != 0xDEADBEEF)
    {    return -1;}
    
    hashTable->dynamicBehaviour = 0;    // to avoid accidental resize while trying to resize

    unsigned int keyCount;
    keyCount = (unsigned int)keycount;            // global variable
    
    char **keysArray = calloc(keyCount, sizeof(char*));     
    void **dataArray = calloc(keyCount,sizeof(void*));    // array storing all data
    void *data;                        // temp data storage
    
    GetKeys(hashTable, &keysArray, &keyCount); // calls GetKeys to store all keys in keysArray 

    // uses FindEntry to get the values for the keys stored in the keysArray
    // puts values in dataArray so that each key-value pair shares same index - easily retrieved
    for (int i = 0; i < keyCount; i++)
    {    FindEntry(hashTable, keysArray[i], (void**)&data);
        dataArray[i] = data;}


    // calculates number of buckets if the useFactor is near the average of contract and expand
    // (contract + expand)/2 = #ofnonempty buckets/number of buckets
    // tries to approximate a value as close as possible to the average of expand and contract
    // minimizes the number of retries resize must do it the new size still results in useFactor being >= expand or <= contract (since if useFactor still invalid, must try again)
    // also ideally reduces the number of times it must dynamically reallocate since its as close to being in the middle of expand and contract as it can be
    float average = ((hashTable->expandUseFactor) + (hashTable->contractUseFactor))/2;
    int nonemptycount = 0;
    for (int i = 0; i < hashTable->Size; i++)
    {
        if ((hashTable->array[i] != NULL) || (hashTable->array[i] != NULL))
        {    nonemptycount++;}
    }

    // uses preserve to not destroy hashTable, but destroy all nodes (ie wipe all trees clean)
    temp_keycount = keycount;
    preserve = 1;
    DestroyHashTable(&hashTable);
    preserve = 0;

    float newsize = ((float)nonemptycount / average) + 1;
    hashTable -> Size = (unsigned char)newsize;

    int returnval = ResizeHelper(hashTable, hashTable -> Size, dataArray, keysArray);
    for (int i = 0; i < temp_keycount; i++)
    {    free(keysArray[i]);}
    free(keysArray);
    return returnval;
}



int ResizeHelper(HashTablePTR hashTable, unsigned char size, void **dataArray, char **keysArray)
{
    hashTable->array = calloc(hashTable->Size, sizeof(struct node));

    void* previousDataHandle = malloc(sizeof(void*));
    for (int i = 0; i < temp_keycount; i++)
    {    InsertEntry (hashTable, keysArray[i], dataArray[i], &previousDataHandle);}

    HashTableInfo *pHashTableInfo = malloc(sizeof(HashTableInfo));
    hashTable -> Size = size;
    GetHashTableInfo(hashTable, pHashTableInfo);
    

    // if a weird case occurs such as expand = 0.5 and contract = 0.4 after only one node is inserted, then it is impossible to properly resize it to obtaina  useFactor > contractUseFactor and < expandUseFactor as you have one nonempty bucket, and thus the amount of buckets must be between 2 and 3. Cannot have fraction buckets. The only way this is possible is if there are 20 buckets with 9 of them being non-empty, but that involves the creation of new buckets -> not resizing. Thus this case is avoided and the Resize behaviour is reset to its initial state (as opposed to terminating the program).
    if ((hashTable->useFactor <= hashTable->contractUseFactor) || (hashTable->useFactor >= hashTable->expandUseFactor))
    {    SetResizeBehaviour (hashTable, 1, (float)0.7, (float)0.2);
        return -1;}

    hashTable -> dynamicBehaviour = 1;
    keycount = temp_keycount;

    free(dataArray);
    free(previousDataHandle);
    free(pHashTableInfo);
    return 0;
}
