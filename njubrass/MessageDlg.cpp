// MessageDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "njubrass.h"
#include "MessageDlg.h"
#include "afxdialogex.h"


// CMessageDlg �Ի���

IMPLEMENT_DYNAMIC(CMessageDlg, CDialog)

CMessageDlg::CMessageDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMessageDlg::IDD, pParent)
{

}

CMessageDlg::~CMessageDlg()
{
}

void CMessageDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CMessageDlg, CDialog)
END_MESSAGE_MAP()


// CMessageDlg ��Ϣ�������
