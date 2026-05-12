#pragma once
#include "CPathItem.h"
#include "CSetupItem.h"
#include "resource.h"
#include <vector>

enum ITEM_TYPE
{
	ITEM_PATH,
	ITEM_SETUP
};

constexpr UINT WM_LISTSCROLL_REMOVE_ITEM = WM_APP + 1;

class CListScrollView : public CScrollView
{
	DECLARE_DYNCREATE(CListScrollView)

public:
	CListScrollView();
	explicit CListScrollView(ITEM_TYPE eItemType);
	virtual ~CListScrollView();

	CDialogEx* AddItem();
	void RemoveItem(int nIndex = -1);
	void RemoveItem(CWnd* pItem);
	void ClearItems();
	virtual void OnInitialUpdate();
	virtual void OnDraw(CDC* pDC) override;

protected:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnRemoveItem(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	CDialogEx* CreateItem();
	void LayoutItems();
	void UpdateScrollSize();
	int GetItemHeight(CWnd* pItem) const;

	ITEM_TYPE m_eItemType;
	std::vector<CDialogEx*> m_vecItems;
};
