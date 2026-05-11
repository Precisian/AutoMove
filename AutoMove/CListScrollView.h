#pragma once
#include "CPathItem.h"
#include "resource.h"
#include <vector>

class CListScrollView : public CScrollView {
    DECLARE_DYNCREATE(CListScrollView)
    std::vector<CPathItem*> m_vecItems;

public:
    void AddItem();
    void RemoveItem(int idx = -1);
    virtual void OnInitialUpdate();
    virtual void OnDraw(CDC* pDC) override;
    virtual void OnSize(UINT nType, int cx, int cy);
};