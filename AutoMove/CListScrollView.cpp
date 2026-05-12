#include "pch.h"
#include "CListScrollView.h"

IMPLEMENT_DYNCREATE(CListScrollView, CScrollView)

BEGIN_MESSAGE_MAP(CListScrollView, CScrollView)
	ON_WM_SIZE()
	ON_MESSAGE(WM_LISTSCROLL_REMOVE_ITEM, &CListScrollView::OnRemoveItem)
END_MESSAGE_MAP()

CListScrollView::CListScrollView()
	: m_eItemType(ITEM_PATH)
{
}

CListScrollView::CListScrollView(ITEM_TYPE eItemType)
	: m_eItemType(eItemType)
{
}

CListScrollView::~CListScrollView()
{
	ClearItems();
}

void CListScrollView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	CSize sizeTotal(0, 10000); // 가로는 0, 세로는 필요한 만큼
	SetScrollSizes(MM_TEXT, sizeTotal);
}

void CListScrollView::OnDraw(CDC* pDC)
{
	CRect rect;
	GetClientRect(&rect);
	pDC->FillSolidRect(rect, RGB(255, 255, 255));
}

void CListScrollView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);
	LayoutItems();
}

CDialogEx* CListScrollView::AddItem()
{
	CDialogEx* pItem = CreateItem();
	if (pItem == nullptr)
	{
		return nullptr;
	}

	m_vecItems.push_back(pItem);
	pItem->ShowWindow(SW_SHOW);

	LayoutItems();
	return pItem;
}

void CListScrollView::RemoveItem(int nIndex)
{
	if (m_vecItems.empty())
	{
		return;
	}

	if (nIndex < 0)
	{
		nIndex = static_cast<int>(m_vecItems.size()) - 1;
	}

	if (nIndex < 0 || nIndex >= static_cast<int>(m_vecItems.size()))
	{
		return;
	}

	CDialogEx* pItem = m_vecItems[nIndex];
	if (pItem != nullptr)
	{
		if (pItem->GetSafeHwnd())
		{
			pItem->DestroyWindow();
		}

		delete pItem;
	}

	m_vecItems.erase(m_vecItems.begin() + nIndex);
	LayoutItems();
}

void CListScrollView::RemoveItem(CWnd* pItem)
{
	if (pItem == nullptr)
	{
		return;
	}

	for (int i = 0; i < static_cast<int>(m_vecItems.size()); ++i)
	{
		if (m_vecItems[i] == pItem)
		{
			RemoveItem(i);
			return;
		}
	}
}

void CListScrollView::ClearItems()
{
	for (int i = 0; i < static_cast<int>(m_vecItems.size()); ++i)
	{
		CDialogEx* pItem = m_vecItems[i];
		if (pItem != nullptr)
		{
			if (pItem->GetSafeHwnd())
			{
				pItem->DestroyWindow();
			}

			delete pItem;
		}
	}

	m_vecItems.clear();
	UpdateScrollSize();
}

LRESULT CListScrollView::OnRemoveItem(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	const HWND hItem = reinterpret_cast<HWND>(wParam);
	if (hItem == nullptr)
	{
		return 0;
	}

	for (int i = 0; i < static_cast<int>(m_vecItems.size()); ++i)
	{
		CDialogEx* pItem = m_vecItems[i];
		if (pItem != nullptr && pItem->GetSafeHwnd() == hItem)
		{
			RemoveItem(i);
			break;
		}
	}

	return 0;
}

CDialogEx* CListScrollView::CreateItem()
{
	CDialogEx* pItem = nullptr;
	UINT nDialogID = 0;

	switch (m_eItemType)
	{
	case ITEM_PATH:
		pItem = new CPathItem(this);
		nDialogID = IDD_PATHITEM_DIALOG;
		break;
	case ITEM_SETUP:
		pItem = new CSetupItem(this);
		nDialogID = IDD_SETUPITEM_DIALOG;
		break;
	default:
		return nullptr;
	}

	if (!pItem->Create(nDialogID, this))
	{
		delete pItem;
		return nullptr;
	}

	return pItem;
}

void CListScrollView::LayoutItems()
{
	if (!GetSafeHwnd())
	{
		return;
	}

	CRect viewRect;
	GetClientRect(&viewRect);

	int nY = 0;
	for (int i = 0; i < static_cast<int>(m_vecItems.size()); ++i)
	{
		CDialogEx* pItem = m_vecItems[i];
		if (pItem == nullptr || !pItem->GetSafeHwnd())
		{
			continue;
		}

		const int nItemHeight = GetItemHeight(pItem);
		pItem->SetWindowPos(nullptr, 0, nY, viewRect.Width(), nItemHeight, SWP_NOZORDER);
		nY += nItemHeight;
	}

	SetScrollSizes(MM_TEXT, CSize(viewRect.Width(), nY));
}

void CListScrollView::UpdateScrollSize()
{
	if (!GetSafeHwnd())
	{
		return;
	}

	CRect viewRect;
	GetClientRect(&viewRect);

	int nHeight = 0;
	for (int i = 0; i < static_cast<int>(m_vecItems.size()); ++i)
	{
		nHeight += GetItemHeight(m_vecItems[i]);
	}

	SetScrollSizes(MM_TEXT, CSize(viewRect.Width(), nHeight));
}

int CListScrollView::GetItemHeight(CWnd* pItem) const
{
	if (pItem == nullptr || !pItem->GetSafeHwnd())
	{
		return 0;
	}

	CRect itemRect;
	pItem->GetWindowRect(&itemRect);
	return itemRect.Height();
}