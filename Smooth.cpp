#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <csttddef>

using namespace std;

//定義平滑運算的次數
#define NSmooth 1000

//type
typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;
typedef int LONG;
 
#pragma pack( push, 1 )                     //struct alignment§K BMPHEADER ³Qsizeof Ū¦¨16 
typedef struct tagBITMAPFILEHEADER {        //(14bytes)
        WORD bfType;                        //(2bytes)         File type, in BMP case, it¡¦ll be ¡¥BM¡¦(0x424D)
        DWORD bfSize;                       //(4bytes)        BMP file size
        WORD bfReserved1;                   //(2bytes)        Always 0
        WORD bfReserved2;                   //(2bytes)        Always 0
        DWORD bfOffbytes;                   //(4bytes)        Size of Headers + Palette, 14 + 40 + 4 * 28 in our case
} BMPHEADER; 
#pragma pack( pop )
 
typedef struct tagBITMAPINFOHEADER{         //(40bytes)
        DWORD biSize;                       //(4bytes)        After Windows 3.X, it¡¦s always 40, which is the structure size of BITMAPINFOHEADER
        LONG biWidth;                       //(4bytes)        The width of image
        LONG biHeight;                      //(4bytes)        The height of image
        WORD biPlanes;                      //(2bytes)        How many images in this file. For BMP, it¡¦s  always 1
        WORD biBitCount;                    //(2bytes)        How many bits stand for a pixel, 8 in our case
        DWORD biCompression;                //(4bytes)        0 is no compression, 1 is 8-bitRLE compression, 2 is 4-bitRLE compression.
                                            //        We only deal with no compression image.
        DWORD biSizeImage;                  //(4bytes)         The image size after compress. If no compression, it could be 0 or image size
        LONG biXPelsPerMeter;               //(4bytes)        horizontal dots per meter
        LONG biYPelsPerMeter;               //(4bytes)        vertical dots per meter 
        DWORD biClrUsed;                    //(4bytes)        How many colors used in palette, 0 for all colors
        DWORD biClrImportant;               //(4bytes)        How many colors are important, 0 for all
} BMPINFO; 
 
typedef struct tagRGBTRIPLE{                //(3bytes)
        BYTE rgbBlue;                       //(1bytes)        blue channel
        BYTE rgbGreen;                      //(1bytes)        green channel
        BYTE rgbRed;                        //(1bytes)        red channel
} RGBTRIPLE; 
/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;
RGBTRIPLE **BMPData = NULL;
RGBTRIPLE **BMPOrigin = NULL ;

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP( char *fileName);        //read file
int saveBMP( char *fileName);        //save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory( int Y, int X );        //allocate memory
int main(int argc,char *argv[])
{
/*********************************************************/
/*變數宣告：                                             */
/*  *infileName  ： 讀取檔名                             */
/*  *outfileName ： 寫入檔名                             */
/*  startwtime   ： 記錄開始時間                         */
/*  endwtime     ： 記錄結束時間                         */
/*********************************************************/
	char *infileName = "input.bmp";
        char *outfileName = "output.bmp";
	double startwtime = 0.0, endwtime=0;

	MPI_Init(&argc,&argv);
        //create datatype
        MPI_Datatype data[3] = {MPI_BYTE , MPI_BYTE , MPI_BYTE};
        MPI_Datatype _type   , type;
        MPI_Type_create_struct(3 , 1 ,{offsetof(RGBTRIPLE , rgbBlue) ,offsetof(RGBTRIPLE , rgbGreen) ,offsetof(RGBTRIPLE , rgbRed) },data ,&_type) ;
        MPI_Type_commit(&_type) ;
        int worker, id ;
        MPI_Comm_rank(MPI_COMM_WORLD , &id) ;
        MPI_Comm_size(MPI_COMM_WORLD , &worker) ;

	//記錄開始時間
	startwtime = MPI_Wtime();

	//讀取檔案

        int height , width ;
        if (id == 0){
                if ( readBMP( infileName) )\
                        cout << "Read file successfully!!" << endl;
                else
                        cout << "Read file fails!!" << endl;
                height = bmpInfo.biHeight / worker ;
                width = bmpInfo.biWidth ;
                MPI_Bcast(&height , 1 , MPI_INT , 0 , MPI_COMM_WORLD);
                MPI_Bcast(&width  , 1 , MPI_INT , 0 , MPI_COMM_WORLD) ;
                height = height  + bmpInfo.biHeight%worker ;
                BMPSaveData = alloc_memory( height + 2, width) ;
        }
        else{
                MPI_Bcast(&height , 1 , MPI_INT , 0 , MPI_COMM_WORLD);
                MPI_Bcast(&width  , 1 , MPI_INT , 0 , MPI_COMM_WORLD);
                BMPSaveData = alloc_memory( height + 2, width) ;
        }
        MPI_Type_contiguous(width , _type , &type) ;
        MPI_Type_commit(&type) ;
	//Scatter
        int * disp = new int[worker] ;
        if (!id)
                disp[0] = height ;
        else
                disp[0] = height + bmpInfo.biHeight/worker ; 
        for(int i = 1 ; i < worker ; i++)
                disp[i] = bmpInfo.biHeight / worker ;

        MPI_Scatterv(BMPOrigin , disp , type , &BMPSaveData[1] , height , type,0, MPI_COMM_WORLD) ;

        
        //動態分配記憶體給暫存空間
        BMPData = alloc_memory( bmpInfo.biHeight + 2, bmpInfo.biWidth);
        //Get 
        //進行多次的平滑運算
	for(int count = 0; count < NSmooth ; count ++){
                // get 
                
                RGBTRIPLE * sent_top  = BMPSaveData[1] ;
                RGBTRIPLE * sent_down = BMPSaveData[height] ;
                RGBTRIPLE * recv_top = BMPSaveData[0];
                RGBTRIPLE * recv_down = BMPSaveData[height + 1];
                //transmit top -> down
                MPI_Sendrecv(&sent_top ,1, (id-1)> 0? id - 1: worker - 1  , type , &recv_top ,1, (id + 1) < (worker - 1) ? id + 1 : 0 , type ,MPI_COMM_WORLD ,MPI_STATUS_IGNORE) ;
                MPI_Barrier(MPI_COMM_WORLD) ;
                MPI_Sendrecv(&sent_down ,1, (id + 1) < (worker - 1) ? id + 1 : 0  , type , &recv_down ,1,(id-1)> 0? id - 1: worker - 1 , type ,MPI_COMM_WORLD ,MPI_STATUS_IGNORE) ;
                
                
		//把像素資料與暫存指標做交換
		swap(BMPSaveData,BMPData);
		//進行平滑運算
		for(int i = 1; i< height + 1; i++)
			for(int j =0; j< width ; j++){
				/*********************************************************/
				/*設定上下左右像素的位置                                 */
				/*********************************************************/
				int Top = i - 1
				int Down = i + 1
				int Left = j>0 ? j-1 :width-1;
				int Right = j< width-1 ? j+1 : 0;
				/*********************************************************/
				/*與上下左右像素做平均，並四捨五入                       */
				/*********************************************************/
				BMPSaveData[i][j].rgbBlue =  (double) (BMPData[i][j].rgbBlue+BMPData[Top][j].rgbBlue+BMPData[Top][Left].rgbBlue+BMPData[Top][Right].rgbBlue+BMPData[Down][j].rgbBlue+BMPData[Down][Left].rgbBlue+BMPData[Down][Right].rgbBlue+BMPData[i][Left].rgbBlue+BMPData[i][Right].rgbBlue)/9+0.5;
				BMPSaveData[i][j].rgbGreen =  (double) (BMPData[i][j].rgbGreen+BMPData[Top][j].rgbGreen+BMPData[Top][Left].rgbGreen+BMPData[Top][Right].rgbGreen+BMPData[Down][j].rgbGreen+BMPData[Down][Left].rgbGreen+BMPData[Down][Right].rgbGreen+BMPData[i][Left].rgbGreen+BMPData[i][Right].rgbGreen)/9+0.5;
				BMPSaveData[i][j].rgbRed =  (double) (BMPData[i][j].rgbRed+BMPData[Top][j].rgbRed+BMPData[Top][Left].rgbRed+BMPData[Top][Right].rgbRed+BMPData[Down][j].rgbRed+BMPData[Down][Left].rgbRed+BMPData[Down][Right].rgbRed+BMPData[i][Left].rgbRed+BMPData[i][Right].rgbRed)/9+0.5;
			}
	}
        MPI_Gather(&BMPSaveData , height , type ,&BMPOrigin , height ,type , 0 , MPI_COMM_WORLD) ;

 	//寫入檔案
        if ( saveBMP( outfileName ) )
                cout << "Save file successfully!!" << endl;
        else
                cout << "Save file fails!!" << endl;

	//得到結束時間，並印出執行時間
        //endwtime = MPI_Wtime();
    	//cout << "The execution time = "<< endwtime-startwtime <<endl ;

	free(BMPData[0]);
 	free(BMPSaveData[0]);
 	free(BMPData);
 	free(BMPSaveData);
        free(BMPOrigin);
 	//MPI_Finalize();

    return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
	//建立輸入檔案物件
        ifstream bmpFile( fileName, ios::in | ios::binary );

        //檔案無法開啟
        if ( !bmpFile ){
                cout << "It can't open file!!" << endl;
                return 0;
        }

        //讀取BMP圖檔的標頭資料
    	bmpFile.read( ( char* ) &bmpHeader, sizeof( BMPHEADER ) );

        //判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }

        //讀取BMP的資訊
        bmpFile.read( ( char* ) &bmpInfo, sizeof( BMPINFO ) );

        //判斷位元深度是否為24 bits
        if ( bmpInfo.biBitCount != 24 ){
                cout << "The file is not 24 bits!!" << endl;
                return 0;
        }

        //修正圖片的寬度為4的倍數
        while( bmpInfo.biWidth % 4 != 0 )
        	bmpInfo.biWidth++;

        //動態分配記憶體
        BMPOrigin = alloc_memory( bmpInfo.biHeight, bmpInfo.biWidth);

        //讀取像素資料
    	//for(int i = 0; i < bmpInfo.biHeight; i++)
        //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
	    bmpFile.read( (char* )BMPOrigin[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight);

        //關閉檔案
        bmpFile.close();

        return 1;

}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP( char *fileName)
{
 	//判決是否為BMP圖檔
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }

 	//建立輸出檔案物件
        ofstream newFile( fileName,  ios:: out | ios::binary );

        //檔案無法建立
        if ( !newFile ){
                cout << "The File can't create!!" << endl;
                return 0;
        }

        //寫入BMP圖檔的標頭資料
        newFile.write( ( char* )&bmpHeader, sizeof( BMPHEADER ) );

	//寫入BMP的資訊
        newFile.write( ( char* )&bmpInfo, sizeof( BMPINFO ) );

        //寫入像素資料
        //for( int i = 0; i < bmpInfo.biHeight; i++ )
        //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
        newFile.write( ( char* )BMPOrigin[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight );

        //寫入檔案
        newFile.close();

        return 1;

}


/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X )
{
	//建立長度為Y的指標陣列
        RGBTRIPLE **temp = new RGBTRIPLE *[ Y ];
	RGBTRIPLE *temp2 = new RGBTRIPLE [ Y * X ];
        memset( temp, 0, sizeof( RGBTRIPLE ) * Y);
        memset( temp2, 0, sizeof( RGBTRIPLE ) * Y * X );

	//對每個指標陣列裡的指標宣告一個長度為X的陣列
        for( int i = 0; i < Y; i++){
                temp[ i ] = &temp2[i*X];
        }

        return temp;

}
/*********************************************************/
/* 交換二個指標                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
	RGBTRIPLE *temp;
	temp = a;
	a = b;
	b = temp;
}
