#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <stdio.h>
#include <vector>
#include <cstring>
#include <cmath>
using namespace std;
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0

typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;

// Structure Initialization
typedef struct _bitfiled {
    unsigned b0 : 1;
    unsigned b1 : 1;
    unsigned b2 : 1;
    unsigned b3 : 1;
    unsigned b4 : 1;
    unsigned b5 : 1;
    unsigned b6 : 1;
    unsigned b7 : 1;
} bitfiled; // for write compressed data

struct DCnode{
    int size = 0;
    string code = "";
    int32 frequency = 0;
    DCnode *leftchild = NULL;
    DCnode *rightchild = NULL;
};

struct ACnode{
    int size = 0;
    int runlength = 0;
    int32 frequency = 0;
    bool visit = false;
    string code = "";
    ACnode *leftchild = NULL;
    ACnode *rightchild = NULL;
};

struct DCparam{
    bool visit = false;
    int size = 0;
    string code = "";
    string value = "";
};

struct RLC{
    bool visit = false;
    int runlength = 0;
    int size = 0;
    int value = 0;
    string code = "";
    string bvalue = ""; // binarized value
};

struct MB{
    int pixel[8][8] = {0};
    int vec[64] = {0};
    vector<RLC> RLCarray;
    DCparam DP;
};

struct DCtreeparam{
    byte size = 0;
    string code = "";
};

// Parameter Initialization
const double PI = std::acos(-1);
int qtable[8][8] = {
        {16,11,10,16,24,40,51,61},
        {12,12,14,19,26,58,60,55},
        {14,13,16,24,40,57,69,56},
        {14,17,22,29,51,87,80,62},
        {18,22,37,56,68,109,103,77},
        {24,35,55,64,81,104,113,92},
        {49,64,78,87,103,121,120,101},
        {72,92,95,98,112,100,103,99}
        };
vector<MB> MBarray;
vector<int> DPCMarray;
vector<DCnode> DCnodearray; // DC node array
vector<ACnode> ACnodearray1;
vector<ACnode> ACnodearray2; // AC node aaray

// Function
void ReadImage(const char* fileName, const char* outfileName, byte** pixels, int32* width, int32* height, int32* bytesPerPixel, FILE*& imageFile, FILE*& OUT)
{
    imageFile = fopen(fileName, "rb");
    int32 dataOffset;
    int32 LookUpTable=0; 
    fseek(imageFile, HEADER_SIZE + INFO_HEADER_SIZE-8, SEEK_SET);
    fread(&LookUpTable, 4, 1, imageFile);
    fseek(imageFile, 0, SEEK_SET);

    OUT = fopen(outfileName, "wb");

    int header = 0;
    if (LookUpTable)
        header = HEADER_SIZE + INFO_HEADER_SIZE + 1024;
    else
        header = HEADER_SIZE + INFO_HEADER_SIZE;
    for (int i = 0; i < header; i++)
    {
        int get = getc(imageFile);
        putc(get, OUT);
    }

    fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET);
    fread(&dataOffset, 4, 1, imageFile);
    fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
    fread(width, 4, 1, imageFile);
    fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
    fread(height, 4, 1, imageFile);
    int16 bitsPerPixel;
    fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
    fread(&bitsPerPixel, 2, 1, imageFile);
    *bytesPerPixel = ((int32)bitsPerPixel) / 8; //3 bytes per pixel when color, 1 byte per pixel when grayscale

    int paddedRowSize = (int)(4 * (float)(*width) / 4.0f) * (*bytesPerPixel);
    int unpaddedRowSize = (*width) * (*bytesPerPixel);
    int totalSize = unpaddedRowSize * (*height);

    *pixels = new byte[totalSize];
    int i = 0;
    byte* currentRowPointer = *pixels + ((*height - 1) * unpaddedRowSize);
    for (i = 0; i < *height; i++)
    {
        fseek(imageFile, dataOffset + (i * paddedRowSize), SEEK_SET);
        fread(currentRowPointer, 1, unpaddedRowSize, imageFile);
        currentRowPointer -= unpaddedRowSize;
    }
    fclose(imageFile);
}

void WriteImage(byte* pixels, int32 width, int32 height, int32 bytesPerPixel, FILE*& outputFile)
{
    int paddedRowSize = (int)(4 * (float)width / 4.0f) * bytesPerPixel;
    int unpaddedRowSize = width * bytesPerPixel;
    for (int i = 0; i < height; i++)
    {
        int pixelOffset = ((height - i) - 1) * unpaddedRowSize;
        // Iterate 1/3 times of #col 
        // stride = 3*i
        for(int i=0;i<paddedRowSize;i++) {
            fwrite(&pixels[pixelOffset+i], 1, 1, outputFile);
        }
    }
    fclose(outputFile);
} // Just check for BMP file is 24bits or 8bits

void readMB(byte* pixels, int32 width, int32 height, int32 bytesPerPixel)
{
    int unpaddedRowSize = width * bytesPerPixel;
    cout << "Row:\t" << height << endl;
    cout << "Col:\t" << unpaddedRowSize << endl;
    MB tempMB;
    int start;
    int col;
    int row;
    for(row=0;row<height;row=row+8)
    {
        for(col=0;col<unpaddedRowSize;col=col+8)
        {
            start = unpaddedRowSize * row + col;
            for(int i=0;i<8;i++)
            {
                for(int j=0;j<8;j++)
                {
                    tempMB.pixel[i][j] = (int)pixels[start+unpaddedRowSize*i+j];
                }
            }
            MBarray.push_back(tempMB);
        }
    }
} // read raw data to Macro Block

void DCT() {
    int length = MBarray.size();
    MB tempMB;
    MB tempMB2;
    float Fuv = 0.0;
    float sumf = 0.0;
    float Cu = 0.0;
    float Cv = 0.0;
    for(int l = 0; l<length; l++)
    {
        tempMB = MBarray[l];
        for(int v=0;v<8;v++)
        {
            if(v == 0) {
                Cv = sqrt(0.5);
            }
            else {
                Cv = 1;
            }
            for(int u=0;u<8;u++)
            {
                if(u == 0) {
                    Cu = sqrt(0.5);
                }
                else {
                    Cu = 1;
                }
                for(int y=0;y<8;y++)
                {
                    for(int x=0;x<8;x++)
                    {
                        sumf = sumf + (tempMB.pixel[x][y]-128) * cos((2.0*x+1.0)*PI*u/16.0) * cos((2.0*y+1.0)*PI*v/16.0);
                    }
                }
                Fuv = 0.25*Cu*Cv*sumf;
                tempMB2.pixel[u][v] = (int)Fuv; // Rounding
                sumf = 0.0; // Refresh
            }
        }
        MBarray[l] = tempMB2;
    }
} // 2Dimensional Discrete Cosine Transform

void quant()
{
    int length = MBarray.size();
    MB tempMB;
    
    for(int l = 0; l<length; l++)
    {
        tempMB = MBarray[l];
        for(int y=0;y<8;y++)
        {
            for(int x=0;x<8;x++)
            {
                tempMB.pixel[x][y] = tempMB.pixel[x][y] / qtable[x][y];
            }
        }
        MBarray[l] = tempMB;
    }
} // Quantization

void zigzag() // Scanning
{
    int length = MBarray.size();
    for(int l=0;l<length;l++)
    {
        MB tempMB;
        int sw = 0;
        int row = 0;
        int col = 0;
        tempMB = MBarray[l];
        for(int i=0; i<64; i++) // 64 times iteration
        {
            tempMB.vec[i] = tempMB.pixel[row][col];
            if(sw == 0) {
                if(col == 7) {
                    row++;
                    sw = 1;
                }
                else {
                    if(row == 0)
                    {
                        col++;
                        sw = 1;
                    }
                    else {
                        row--;
                        col++;
                    }
                }
            }
            else {
                if(row == 7) {
                    col++;
                    sw = 0;
                }
                else {
                    if(col == 0)
                    {
                        row++;
                        sw = 0;
                    }
                    else {
                        col--;
                        row++;
                    }
                }
            }
        }
        MBarray[l] = tempMB;
    }
} // Zig-Zag scanning

void DPCM() {
    int length = MBarray.size();
    DPCMarray.push_back(MBarray[0].vec[0]);
    for(int l=1; l<length; l++)
    {
        DPCMarray.push_back(MBarray[l].vec[0] - MBarray[l-1].vec[0]);
    }
} // Diffrential Pulse Code Modulation

void DCbinaryConverter() {
    int length = DPCMarray.size();
    int size = 0;
    int value = 0;
    // size first value last
    for(int i=0; i<length; i++)
    {
        value = DPCMarray[i];
        // size
        if(abs(value) == 0)
        {
            size = 0;
        }
        else if(abs(value) == 1){
            size = 1;
        }
        else {
            if(value % 2 == 0) {
                size = log2(abs(value)) + 1;
            }
            else {
                size = ceil(log2(abs(value)));
            }
        }
        MBarray[i].DP.size = size;
        // value
        if(value == 0) {
            MBarray[i].DP.value.append("0");
        }
        else if(value > 0) {
            while(1) {
                if(value == 1) {
                    MBarray[i].DP.value.append("1");
                    break;
                }
                else {
                    MBarray[i].DP.value.append(to_string(value%2));
                    value = value / 2;
                }
            }
            char *a = new char[size];
            strcpy(a,MBarray[i].DP.value.c_str());
            for(int j=0;j<size;j++) {
                MBarray[i].DP.value[j] = a[size-1-j]; 
            }
            delete[] a;
        }
        else {
            value = abs(value);
            while(1) {
                if(value == 1) {
                    MBarray[i].DP.value.append("0");
                    break;
                }
                else {
                    int temp = 0;
                    if(value%2 == 0) {
                        temp = 1;
                    }
                    else {
                        temp = 0;
                    }
                    MBarray[i].DP.value.append(to_string(temp));
                    value = value / 2;
                }
            }
            char *a = new char[size];
            strcpy(a,MBarray[i].DP.value.c_str());
            for(int j=0;j<size;j++) {
                MBarray[i].DP.value[j] = a[size-1-j]; 
            }
            delete[] a;
        }
    }
} // Extract size for Dc node and Change value to Binary format

DCnode DCextractMin()
{
	int32 min = UINT32_MAX;
	vector<DCnode>::iterator iter, position;
	for (iter = DCnodearray.begin(); iter != DCnodearray.end(); iter++)
	{
		if (min > (*iter).frequency)
		{
			position = iter;
			min = (*iter).frequency;
		}
	}
	DCnode tempNode = (*position);
	DCnodearray.erase(position);
	return tempNode;
} // Extract node

DCnode DCgetHuffmanTree()
{
	while (!DCnodearray.empty())
	{
		DCnode* tempNode = new DCnode;
		DCnode* tempNode1 = new DCnode;
		DCnode* tempNode2 = new DCnode;
		*tempNode1 = DCextractMin();
		*tempNode2 = DCextractMin();

		tempNode->leftchild = tempNode1;
		tempNode->rightchild = tempNode2;
		tempNode->frequency = tempNode1->frequency + tempNode2->frequency;
		DCnodearray.push_back(*tempNode);

		if (DCnodearray.size() == 1)
			break;
	}
	return DCnodearray[0];
} // Extract root node and make relationship of DC node family

void DCDFS(DCnode* tempRoot, string s)
{
	DCnode* root1 = tempRoot; 
	root1->code = s; 

	if (root1 == NULL)
	{

	} 
	else if (root1->leftchild == NULL && root1->rightchild == NULL)
	{
		cout << "size:\t" << root1->size << "\t" << "code:\t" << root1->code << endl; 
	} 
	else
	{
		root1->leftchild->code = s.append("0"); 
		s.erase(s.end() - 1); 
		root1->rightchild->code = s.append("1"); 
		s.erase(s.end() - 1); 

		DCDFS(root1->leftchild, s.append("0")); 
		s.erase(s.end() - 1); 
		DCDFS(root1->rightchild, s.append("1")); 
		s.erase(s.end() - 1);
	}
} // Code for DC node

DCnode* findDC(DCnode* root, int target_size){
    // DFS
    DCnode* temp = root;
    DCnode* target_node = NULL;
    DCnode* target1 = NULL;
    DCnode* target2 = NULL;
    if(temp == NULL){

    }
    else if(temp->leftchild == NULL && temp->rightchild == NULL) {
        if(temp->size == target_size) {
            target_node = temp;
        }
    }
    else {
        target1 = findDC(temp->leftchild, target_size);
        target2 = findDC(temp->rightchild, target_size);
    }

    if(target1 != NULL) target_node = target1;
    else if(target2 != NULL) target_node = target2;

    return target_node;
} // Find DC node that has target_size

void RLCcoding(){
    int length = MBarray.size();
    int rlcvec[63];
    int rlcCNT = 0;
    RLC temp;
    for(int i=0;i<length;i++) {
        for(int j=1;j<64;j++) {
            rlcvec[j-1] = MBarray[i].vec[j];
        }
        rlcCNT = 0;
        for(int j=0;j<63;j++) {
            if(rlcCNT == 15) {
                temp.runlength = rlcCNT;
                temp.value = rlcvec[j];
                rlcCNT = 0;
                MBarray[i].RLCarray.push_back(temp);
            }
            else if(rlcvec[j] != 0) {
                temp.runlength = rlcCNT;
                temp.value = rlcvec[j];
                rlcCNT = 0;
                MBarray[i].RLCarray.push_back(temp);
            }
            else {
                rlcCNT++;
            }
        }
    }
} // Run Length Coding for each Macro Blocks

void RLCend() {
    int length = MBarray.size();
    for(int i=0; i<length; i++) {
        for(int j = MBarray[i].RLCarray.size()-1; j>0; j--) {
            if(MBarray[i].RLCarray[j-1].value == 0 && MBarray[i].RLCarray[j].value == 0) {
                MBarray[i].RLCarray.erase(MBarray[i].RLCarray.begin() + j);
            }
            else if(MBarray[i].RLCarray[j].value == 0) {
                MBarray[i].RLCarray[j].runlength = 0;
                break;
            }
            if(MBarray[i].RLCarray.size() == 1) {
                MBarray[i].RLCarray[0].runlength = 0;
            }
        }
    }
} // Change for terminate code

void ACbinaryConverter() {
    int length = MBarray.size();
    int size = 0;
    int value = 0;
    // size first value last
    for(int i=0; i<length; i++)
    {
        for(int j=0;j<MBarray[i].RLCarray.size();j++)
        {
            value = MBarray[i].RLCarray[j].value;
            // size
            if(abs(value) == 0)
            {
                size = 0;
            }
            else if(abs(value) == 1){
                size = 1;
            }
            else {
                if(value % 2 == 0) {
                    size = log2(abs(value)) + 1;
                }
                else {
                    size = ceil(log2(abs(value)));
                }
            }
            MBarray[i].RLCarray[j].size = size;

            // value
            if(value == 0) {
                MBarray[i].RLCarray[j].bvalue.append("0");
            }
            else if(value > 0) {
                while(1) {
                    if(value == 1) {
                        MBarray[i].RLCarray[j].bvalue.append("1");
                        break;
                    }
                    else {
                        MBarray[i].RLCarray[j].bvalue.append(to_string(value%2));
                        value = value / 2;
                    }
                }
                char *a = new char[size];
                strcpy(a,MBarray[i].RLCarray[j].bvalue.c_str());
                for(int k=0;k<size;k++) {
                    MBarray[i].RLCarray[j].bvalue[k] = a[size-1-k]; 
                }
                delete[] a;
            }
            else {
                value = abs(value);
                while(1) {
                    if(value == 1) {
                        MBarray[i].RLCarray[j].bvalue.append("0");
                        break;
                    }
                    else {
                        int temp = 0;
                        if(value%2 == 0) {
                            temp = 1;
                        }
                        else {
                            temp = 0;
                        }
                        MBarray[i].RLCarray[j].bvalue.append(to_string(temp));
                        value = value / 2;
                    }
                }
                char *a = new char[size];
                strcpy(a,MBarray[i].RLCarray[j].bvalue.c_str());
                for(int k=0;k<size;k++) {
                    MBarray[i].RLCarray[j].bvalue[k] = a[size-1-k]; 
                }
                delete[] a;
            }
        }
    }
} // Extract size for AC node and change value to binary format

ACnode ACextractMin()
{
	int32 min = UINT32_MAX;
	vector<ACnode>::iterator iter, position;
	for (iter = ACnodearray2.begin(); iter != ACnodearray2.end(); iter++)
	{
		if (min > (*iter).frequency)
		{
			position = iter;
			min = (*iter).frequency;
		}
	}
	ACnode tempNode = (*position);
	ACnodearray2.erase(position);
	return tempNode;
} // Extract AC node

ACnode ACgetHuffmanTree()
{
	while (!ACnodearray2.empty())
	{
		ACnode* tempNode = new ACnode;
		ACnode* tempNode1 = new ACnode;
		ACnode* tempNode2 = new ACnode;
		*tempNode1 = ACextractMin();
		*tempNode2 = ACextractMin();

		tempNode->leftchild = tempNode1;
		tempNode->rightchild = tempNode2;
		tempNode->frequency = tempNode1->frequency + tempNode2->frequency;
		ACnodearray2.push_back(*tempNode);

		if (ACnodearray2.size() == 1)
			break;
	}
	return ACnodearray2[0];
} // Extract root node and make a relationship of AC node family

void ACDFS(ACnode* tempRoot, string s)
{
	ACnode* root1 = tempRoot; 
	root1->code = s; 

	if (root1 == NULL)
	{

	} 
	else if (root1->leftchild == NULL && root1->rightchild == NULL)
	{
		cout << "runlength:\t" << root1->runlength << "\t" << "size:\t" << root1->size << "\t" << "code:\t" << root1->code << endl; 
	} 
	else
	{
		root1->leftchild->code = s.append("0"); 
		s.erase(s.end() - 1); 
		root1->rightchild->code = s.append("1"); 
		s.erase(s.end() - 1); 

		ACDFS(root1->leftchild, s.append("0")); 
		s.erase(s.end() - 1); 
		ACDFS(root1->rightchild, s.append("1")); 
		s.erase(s.end() - 1);
	}
} // code for AC node family

ACnode* findAC(ACnode* root, int target_size, int target_runlength){
    // DFS
    ACnode* temp = root;
    ACnode* target_node = NULL;
    ACnode* target1 = NULL;
    ACnode* target2 = NULL;
    if(temp == NULL){

    }
    else if(temp->leftchild == NULL && temp->rightchild == NULL) {
        if(temp->size == target_size && temp->runlength == target_runlength) {
            target_node = temp;
        }
    }
    else {
        target1 = findAC(temp->leftchild, target_size, target_runlength);
        target2 = findAC(temp->rightchild, target_size, target_runlength);
    }

    if(target1 != NULL) target_node = target1;
    else if(target2 != NULL) target_node = target2;

    return target_node;
} // find the AC node that has target_size and target_runlength

// Main
int main()
{
    byte* pixels;
    int32 width;
    int32 height;
    int32 bytesPerPixel;
    FILE* imageFile;
    FILE* outputFile;
    ReadImage("./JPEG/Lena.bmp", "./JPEG/Lena_de.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile);

    // MB generate
    readMB(pixels,width,height,bytesPerPixel);

    // Check MB
    cout << "Number of Macro-Block:\t" << MBarray.size() << endl;
    cout << "========================= Read to MB =========================\n";
    MB tempMB = MBarray[100];
    cout << "MB[100]" << endl;
    for(int i=0;i<8;i++)
    {
        for(int j=0;j<8;j++)
        {
            cout << tempMB.pixel[i][j] << "\t";
        }
        cout << "\n";
    }

    // DCT
    cout << "============================ 2DCT ============================\n";
    DCT();
    tempMB = MBarray[100];
    cout << "MB[100]" << endl;
    for(int i=0;i<8;i++)
    {
        for(int j=0;j<8;j++)
        {
            cout << tempMB.pixel[i][j] << "\t";
        }
        cout << "\n";
    }

    // qutnziation
    cout << "======================== Quantization ========================\n";
    quant();
    tempMB = MBarray[100];
    cout << "MB[100]" << endl;
    for(int i=0;i<8;i++)
    {
        for(int j=0;j<8;j++)
        {
            cout << tempMB.pixel[i][j] << "\t";
        }
        cout << "\n";
    }

    // Zig-Zag ordering
    cout << "======================== Zig-Zag Scan ========================\n";
    zigzag();
    tempMB = MBarray[100];
    cout << "MB[100]" << endl;
    for(int i=0;i<64;i++)
    {
        if(i == 63) {
            cout << tempMB.vec[i] << "\n";
        }
        else {
            cout << tempMB.vec[i] << ",";
        }
    }

    // DPCM
    cout << "============================ DPCM ============================\n";
    DPCM();
    cout << "DPCMarray Size:\t" << DPCMarray.size() << "\n";
    cout << "First 16 DC value:\t";
    for(int i=0;i<16;i++)
    {
        if(i==15) {
            cout << MBarray[i].vec[0] << "\n";
        }
        else {
            cout << MBarray[i].vec[0] << ",";
        }
    }
    cout << "First 16 DPCM value:\t";
    for (int i=0;i<16;i++)
    {
        if(i==15) {
            cout << DPCMarray[i] << "\n";
        }
        else {
            cout << DPCMarray[i] << ",";
        }
    }

    DCbinaryConverter();
    cout << "First 16 DC size and value-binary\n";
    for(int i=0;i<16;i++)
    {
        cout << "size:\t" << MBarray[i].DP.size << "\t" << "value:\t" << MBarray[i].DP.value << "\n";
    }
    // DC Huffman tree generation
    for(int i=0; i<MBarray.size()-1; i++)
    {
        if(MBarray[i].DP.visit == false) {
            DCnode tempnode;
            tempnode.size = MBarray[i].DP.size;
            tempnode.frequency = 1;
            MBarray[i].DP.visit = true;
            for(int j=i+1;j<MBarray.size();j++) {
                if(MBarray[i].DP.size == MBarray[j].DP.size) {
                    MBarray[j].DP.visit = true;
                    tempnode.frequency++;
                }
            }
            DCnodearray.push_back(tempnode);
        }
    }

    cout << "============================ DC Get Frequency ============================\n";
    int32 sumfreq = 0;
    cout << "DCnodearraySize:\t" << DCnodearray.size() << endl;
    for(int i=0; i<DCnodearray.size(); i++) {
        cout << "size:\t" << DCnodearray[i].size << "\t" << "frequency:\t" << DCnodearray[i].frequency << "\n";
        sumfreq = sumfreq + DCnodearray[i].frequency;
    }
    cout << "Sum of freq:\t" << sumfreq << "\n";

    cout << "============================ DC Huffman Tree ============================\n";
    DCnode DCroot = DCgetHuffmanTree();
    DCDFS(&DCroot,"");

    for(int i=0;i<MBarray.size();i++) {
        DCnode *temp = findDC(&DCroot, MBarray[i].DP.size);
        if(temp == NULL) {
            cout << "Error!\n";
            break;
        }
        else {
            MBarray[i].DP.code = temp->code;
        }
    }
    cout << "======================= First 16 DC encoded size =======================\n";
    for(int i=0; i<16; i++) {
        cout << "code:\t" << MBarray[i].DP.code << "\n";
    }

    // Run-Length
    cout << "======================= RLC coding =======================\n";
    RLCcoding();
    tempMB = MBarray[100];
    cout << "First MB AC value\n";
    for(int i=1;i<64;i++){
        if(i==63) {
            cout << tempMB.vec[i] << "\n";
        }
        else {
            cout << tempMB.vec[i] << ",";
        }
    }

    cout << "First MB RLC\n";
    for(int i=0;i<MBarray[100].RLCarray.size();i++){
        cout << "(" << MBarray[100].RLCarray[i].runlength << "," << MBarray[100].RLCarray[i].value << ")";
    }
    cout << "\n";
    RLCend();
    cout << "First MB RLC-end modified\n";
    for(int i=0;i<MBarray[100].RLCarray.size();i++){
        cout << "(" << MBarray[100].RLCarray[i].runlength << "," << MBarray[100].RLCarray[i].value << ")";
    }
    cout << "\n";
    ACbinaryConverter();
    cout << "First MB RLC-binary\n";
    for(int i=0;i<MBarray[100].RLCarray.size();i++){
        cout << "size:\t" << MBarray[100].RLCarray[i].size << "\t" << "value:\t" << MBarray[100].RLCarray[i].bvalue << "\n";
    }

    // AC Huffman tree generation
    for(int i=0; i<MBarray.size();i++)
    {
        for(int j=0; j<MBarray[i].RLCarray.size();j++) {
            if(MBarray[i].RLCarray[j].visit == false) {
                ACnode tempnode;
                tempnode.size = MBarray[i].RLCarray[j].size;
                tempnode.runlength = MBarray[i].RLCarray[j].runlength;
                tempnode.frequency = 1;
                MBarray[i].RLCarray[j].visit = true;
                if(MBarray[i].RLCarray.size() > 1) {
                    for(int k=j+1;k<MBarray[i].RLCarray.size();k++) {
                        if(MBarray[i].RLCarray[j].size == MBarray[i].RLCarray[k].size && MBarray[i].RLCarray[j].runlength == MBarray[i].RLCarray[k].runlength) {
                            MBarray[i].RLCarray[k].visit = true;
                            tempnode.frequency++;
                        }
                    }
                }
            ACnodearray1.push_back(tempnode);
            }
        }
    }
    for(int i=0; i<ACnodearray1.size()-1;i++) {
        if(ACnodearray1[i].visit == false) {
            ACnode tempnode;
            tempnode.size = ACnodearray1[i].size;
            tempnode.runlength = ACnodearray1[i].runlength;
            tempnode.frequency = ACnodearray1[i].frequency;
            ACnodearray1[i].visit = true;
            for(int j=i+1; j<ACnodearray1.size(); j++) {
                if(ACnodearray1[i].size == ACnodearray1[j].size && ACnodearray1[i].runlength == ACnodearray1[j].runlength) {
                    ACnodearray1[j].visit = true;
                    tempnode.frequency = tempnode.frequency + ACnodearray1[j].frequency;
                }
            }
        ACnodearray2.push_back(tempnode);
        }
    }
    cout << "============================ AC Get Frequency ============================\n";
    sumfreq = 0;
    cout << "ACnodearraySize:\t" << ACnodearray2.size() << endl;
    for(int i=0; i<ACnodearray2.size(); i++) {
        cout << "runglength:\t" << ACnodearray2[i].runlength << "\tsize:\t" << ACnodearray2[i].size << "\t" << "frequency:\t" << ACnodearray2[i].frequency << "\n";
        sumfreq = sumfreq + ACnodearray2[i].frequency;
    }
    cout << "Sum of freq:\t" << sumfreq << "\n";
    cout << "============================ AC Huffman Tree ============================\n";
    ACnode ACroot = ACgetHuffmanTree();
    ACDFS(&ACroot,"");

    for(int i=0;i<MBarray.size();i++) {
        for(int j=0;j<MBarray[i].RLCarray.size();j++) {
            ACnode *temp = findAC(&ACroot, MBarray[i].RLCarray[j].size, MBarray[i].RLCarray[j].runlength);
            if(temp == NULL) {
                cout << "Error!\n";
                break;
            }
            else {
                MBarray[i].RLCarray[j].code = temp->code;
            }
        }
    }
    cout << "======================= First MB encoded size and runlength =======================\n";
    for(int i=0; i<MBarray[0].RLCarray.size();i++) {
        cout << "code:\t" << MBarray[0].RLCarray[i].code << "\n";
    }

    // Entropy coding
    string DCstream = "";
    for(int i=0;i<MBarray.size();i++) {
        DCstream.append(MBarray[i].DP.code);
        DCstream.append(MBarray[i].DP.value);
    } // DC bit stream
    string ACstream = "";
    for(int i=0;i<MBarray.size();i++) {
        for(int j=0; j<MBarray[i].RLCarray.size();j++) {
            ACstream.append(MBarray[i].RLCarray[j].code);
            ACstream.append(MBarray[i].RLCarray[j].bvalue);
        }
    }
    string tot = "";
    tot.append(DCstream);
    tot.append(ACstream);
    cout << "======================= Encoded total bits(except Huffman tree(DC/AC), Qtable) =======================\n";
    int32 col = width * bytesPerPixel;
    int32 row = height;
    int32 origin_bits = 8*col*row;
    cout << "Original Bits:\t\t" << origin_bits << "\n";
    cout << "Compressed Bits:\t" << tot.size() << "\n";
    float Ratio = (float)origin_bits/tot.size();
    cout << "Compression Ratio:\t" << Ratio << "\n";

    // Write each 1bit -> Impossible. Minimum unit is 8bit. Must have write tree's code by myself. But, compressed data will be writed using bitfield.
    // Qtable size : 8bits, Qtable's eact data : 8bits
    // #DC node : 8bits, DC size : 8bits, DC code size(Effective length for each code) : 8bits, DC code : 8bits
    // #AC node : 8bits, AC runlength : 8bits, size : 8bits, AC code size(Effective length for each code), AC code : 16bits

    FILE* pFILE;
    pFILE = fopen("./JPEG/Lena.dat","wb");

    // code
    cout << "Marginal bits considerd!\nC/C++ read/wrtie minimum 8bits, rather then 1bit\n";
    unsigned char DCmargin = 8 - DCstream.size() % 8;
    if(DCmargin != 0) {
        for(int i=0; i<DCmargin; i++) {
            DCstream.append("0");
        }
    }
    int32 DClength = DCstream.size();
    printf("DClength:\t\t%d\n",DClength);
    printf("DCmargin:\t\t%d\n", DCmargin);
    
    unsigned char ACmargin = 8 - ACstream.size() % 8;
    if(ACmargin != 0) {
        for(int i=0; i< ACmargin; i++) {
            ACstream.append("0");
        }
    }
    int32 AClength = ACstream.size();
    printf("AClength:\t\t%d\n",AClength);
    printf("ACmargin:\t\t%d\n", ACmargin);

    tot = "";
    tot.append(DCstream);
    tot.append(ACstream);

    fwrite(&DClength,4,1,pFILE); // DCstream length
    fwrite(&DCmargin,1,1,pFILE); // #DCstream's marginal bits
    fwrite(&AClength,4,1,pFILE); // ACstream length
    fwrite(&ACmargin,1,1,pFILE); // #ACstream's marginal bits

    for(int i=0; i<tot.size();i=i+8) {
        bitfiled bit;
        memset(&bit, 0, sizeof(bitfiled));
        bit.b0 = tot[i];
        bit.b1 = tot[i+1];
        bit.b2 = tot[i+2];
        bit.b3 = tot[i+3];
        bit.b4 = tot[i+4];
        bit.b5 = tot[i+5];
        bit.b6 = tot[i+6];
        bit.b7 = tot[i+7];
        unsigned char cc = (bit.b0 * 128  + bit.b1 * 64 + bit.b2 * 32 + bit.b3 * 16 + bit.b4 * 8 + bit.b5 * 4 + bit.b6 * 2 + bit.b7);
        fwrite(&cc,1,1,pFILE);
    } // data. If you write tot each by each, then, data will have 8times more size.

    fclose(pFILE);

    // Decoding
    // Start from Runlength

    for(int i=0;i<MBarray.size();i++) { // Decompress RLC 
        int temp = 0;
        for(int j=0;j<MBarray[i].RLCarray.size();j++) {
            temp += (MBarray[i].RLCarray[j].runlength + 1);
            MBarray[i].vec[temp] = MBarray[i].RLCarray[j].value;
        }
        temp = 0;
    }
    
    for(int i=1;i<DPCMarray.size();i++) { // Decompress DPCM
            DPCMarray[i] = DPCMarray[i] + DPCMarray[i-1];
            MBarray[i].vec[0] = DPCMarray[i];
    }

    for(int l=0;l<MBarray.size();l++) { // Decompress Zig-Zag
            MB tempMB;
            int sw = 0;
            int row = 0;
            int col = 0;
            tempMB = MBarray[l];
    
            for(int i=0; i<64; i++) // 64 times iteration
            {
                tempMB.pixel[row][col] = tempMB.vec[i];
                if(sw == 0) {
                    if(col == 7) {
                        row++;
                        sw = 1;
                    }
                    else {
                        if(row == 0)
                        {
                            col++;
                            sw = 1;
                        }
                        else {
                            row--;
                            col++;
                        }
                    }
                }
                else {
                    if(row == 7) {
                        col++;
                        sw = 0;
                    }
                    else {
                        if(col == 0)
                        {
                            row++;
                            sw = 0;
                        }
                        else {
                            col--;
                            row++;
                        }
                    }
                }
            }
            MBarray[l] = tempMB;
        }

        for(int i=0;i<MBarray.size();i++) { // Decompress Quantization
            for(int j=0;j<8;j++) {
                for(int k=0;k<8;k++) {
                    MBarray[i].pixel[j][k] = MBarray[i].pixel[j][k] * qtable[j][k];
                }
            }
        }

        for(int i=0;i<MBarray.size();i++) { // IDCD
            MB tempMB;
            MB tempMB2;
            float Fuv = 0.0;
            float sumf = 0.0;
            float Cu = 0.0;
            float Cv = 0.0;
            tempMB = MBarray[i];
            for(int y=0;y<8;y++)
            {
                for(int x=0;x<8;x++)
                {
                    for(int v=0;v<8;v++)
                    {
                        if(v == 0) {
                            Cv = sqrt(0.5);
                        }
                        else {
                            Cv = 1;
                        }
                        for(int u=0;u<8;u++)
                        {
                            if(u == 0) {
                                Cu = sqrt(0.5);
                            }
                            else {
                                Cu = 1;
                            }
                            sumf += + 0.25*Cu*Cv*((tempMB.pixel[u][v]) * cos((2.0*x+1.0)*PI*u/16.0) * cos((2.0*y+1.0)*PI*v/16.0));
                        }
                    }
                    tempMB2.pixel[x][y] = (int)sumf; // Rounding
                    sumf = 0.0; // Refresh
                }
            }
            MBarray[i] = tempMB2;
        }

        for(int i=0;i<MBarray.size();i++) { // Add 128
            for(int x=0;x<8;x++) {
                for(int y=0;y<8;y++) {
                    MBarray[i].pixel[x][y] += 128;
                }
            }
        }

        for(int i=0;i<height;i=i+8) // Pixels Change
        {
            int start;
            int unpaddedRowSize = width * bytesPerPixel;
            for(int j=0;j<unpaddedRowSize;j=j+8)
            {
                start = unpaddedRowSize * i + j;
                MB temp = MBarray[0];
                for(int x=0;x<8;x++)
                {
                    for(int y=0;y<8;y++)
                    {
                        pixels[start+unpaddedRowSize*x+y] = temp.pixel[x][y];
                    }
                }
                MBarray.erase(MBarray.begin());
            }
        }

        WriteImage(pixels, width, height, bytesPerPixel, outputFile);

    

    delete[] pixels;
    return 0;
}