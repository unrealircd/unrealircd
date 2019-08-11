typedef struct {
	unsigned int CurrLoclUsers;
	unsigned int CurrGlobUsers;
	unsigned int MaxLoclUsers;
	unsigned int MaxGlobUsers;
	unsigned int NumUsers; // Eh ??
	unsigned int Invisible;
	unsigned int connections;
	unsigned int NumIRCops;
	unsigned int LocalClients; // Eh??
	unsigned int LocalServers;
	unsigned int chans;
	unsigned int Servers;

	/* ToBe Added
	int TSsync stuff .... dont know what yet
	*/
} WINSTATS, *pWINSTATS;

typedef struct RichLine
{
	BYTE	*Data;
	WORD	Len;
	struct RichLine	*Prev, *Next;
} aRichLine;

typedef struct AllRichLines
	{
	aRichLine *First, *Current;
	int NumLines;
	} INFRICHLINE;



/**********************************************************
********** Graphy Header (C) David Flynn 2000 ************/

#define SS_NORM                0x0001  // spincube window styles
#define SS_SLOW             0x0002
#define SS_FAST             0x0003

#define CCHSTYLE                20  // size of style string, i.e. "SS_ERASE"

#define NUM_GRAPH_STYLES     2


#define GRAPH_EXTRA          4   // number of extra bytes for spincube class


#define IDS_REGCLASSFAIL      16
#define IDS_UNREGFAIL         17
#define IDS_DLGBOXFAIL        18
#define IDS_ALLOCFAIL         19
#define IDS_CREATEDCFAIL      20
#define IDS_CREATEBITMAPFAIL  21
#define GWL_GRAPHDATA        0   
// offset of control's instance data

#define GRAPHCLASS           "Graph"
#define GRAPHDESCRIPTION     "An animated control"
#define GRAPHDEFAULTTEXT     ":-)"
#define GRAPH_EVENT				1
#define UPDATE_TIMER			2
#define UPDATE_INTERVAL			/*60000*/ 30

typedef struct
{
  HDC      hdcCompat;               // the DC that will contain our off-screen
                                    //   image
  //HBITMAP  hbmSave;                 // Save previous selected bitmap
  //HBITMAP  hbmCompat;               // The bitmap that will contain the actual
                                    //   image, i.e. we will always do our
                                    //   drawing on this bmp & then blt the
                                    //   result to the screen.
  BOOL InitDraw;  	//Did we draw once yet?
  int width;            // Width of monitor (= size of history)
  int cpupointer;       // pointer to cpu history

  HDC DCBack;
  HBITMAP BMBack;
  HBITMAP OldBack;
  HDC DCDblBuff;
  HBITMAP BMDblBuff;
  HBITMAP OldDblBuff;

  HBITMAP	Background;
  HBITMAP	CPUMap;
  SIZE WindowSize;
  BOOL Border;
  BOOL Grid;

  COLORREF BorderColor;
  COLORREF BackColor;
  COLORREF GridColor;
  COLORREF CPUColor;
  COLORREF AVGCPUColor;
  COLORREF MEMColor;
  char cpuhistory[2048];
  char BackgroundPath[256];
char CPUMapPath[256];
  int      iOptions;                // Contains the current options for this
                                    //   ctrl, i.e. erase background.

} GRAPHINFO, *PGRAPHINFO;



/******************************************************************************\
*                                FUNCTION PROTOTYPES
\******************************************************************************/

LRESULT CALLBACK GraphWndProc    (HWND,  UINT,  WPARAM, LPARAM);







DWORD Reserved,dataType,dataLen=8192;



// DoubleBuffer Stuff
void DrawMonitor(HDC hdc, RECT r,HWND);


void CreateDblBuff(HWND);
void FreeDblBuff(void);
