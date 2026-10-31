// TmcRTH_Input.cpp : implementation file
//

#include "STDAFX.H"
#include <TmcRTH_Input.h>

//#ifdef _DEBUG
//#define new DEBUG_NEW
//#undef THIS_FILE
//static char THIS_FILE[] = __FILE__;
//#endif

/////////////////////////////////////////////////////////////////////////////
// CTmcRTH_Input

CTmcRTH_Input::CTmcRTH_Input()
{
	pcInputNode = NULL;
	nType = 0;
	nNode = 0;
	rX = (_real)(0.0);
	rY = (_real)(0.0);
	return;
}

CTmcRTH_Input::~CTmcRTH_Input()
{
	DeleteData();
	return;
}




/////////////////////////////////////////////////////////////////////////////
// CTmcRTH_Input message handlers

void CTmcRTH_Input::DeleteData( void )
{
	if( pcInputNode != NULL ) delete []pcInputNode;
	pcInputNode = NULL;
	nType = 0;
	nNode = 0;
	rX = (_real)(0.0);
	rY = (_real)(0.0);
	return;
}
