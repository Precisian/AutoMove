#include "pch.h"
#include "CListScrollView.h"

IMPLEMENT_DYNCREATE(CListScrollView, CScrollView)

void CListScrollView::OnInitialUpdate() {
    CScrollView::OnInitialUpdate();
    // 초기에는 스크롤 범위를 0으로 설정 (스크롤바가 안 보임)
    SetScrollSizes(MM_TEXT, CSize(0, 0));
}

void CListScrollView::OnDraw(CDC* pDC) {
    // 배경을 흰색으로 칠하고 싶을 때 (선택 사항)
    CRect rect;
    GetClientRect(&rect);
    pDC->FillSolidRect(rect, RGB(255, 255, 255));
}

void CListScrollView::OnSize(UINT nType, int cx, int cy) {
    CScrollView::OnSize(nType, cx, cy);

    // 생성된 모든 아이템의 너비를 현재 뷰의 너비(cx)로 일괄 변경
    for (size_t i = 0; i < m_vecItems.size(); ++i) {
        if (m_vecItems[i] && m_vecItems[i]->GetSafeHwnd()) {
            CRect itemRect;
            m_vecItems[i]->GetWindowRect(&itemRect);
            int nItemHeight = itemRect.Height();

            m_vecItems[i]->SetWindowPos(NULL, 0, (int)i * nItemHeight, cx, nItemHeight, SWP_NOZORDER);
        }
    }
}

void CListScrollView::AddItem() {
    CPathItem* pItem = new CPathItem();
    if (!pItem->Create(IDD_PATHITEM_DIALOG, this)) {
        delete pItem;
        return;
    }

    // 1. 현재 스크롤뷰의 클라이언트 영역 크기를 구함
    CRect viewRect;
    GetClientRect(&viewRect);

    // 2. 아이템 다이얼로그의 기본 높이를 구함
    CRect itemRect;
    pItem->GetWindowRect(&itemRect);
    int nItemHeight = itemRect.Height();

    // 3. 위치 계산 (nYPos)
    int nYPos = (int)m_vecItems.size() * nItemHeight;

    // 4. SetWindowPos를 호출할 때 너비(cx)를 스크롤뷰의 너비(viewRect.Width())로 설정
    // SWP_NOSIZE를 제거해야 크기가 변경됩니다.
    pItem->SetWindowPos(NULL, 0, nYPos, viewRect.Width(), nItemHeight, SWP_NOZORDER);

    pItem->ShowWindow(SW_SHOW);
    m_vecItems.push_back(pItem);

    // 5. 스크롤 영역 설정 (가로는 viewRect.Width()로, 세로는 전체 합으로)
    CSize sizeTotal(viewRect.Width(), (int)m_vecItems.size() * nItemHeight);
    SetScrollSizes(MM_TEXT, CSize(viewRect.Width(), (int)m_vecItems.size() * nItemHeight));
}

void CListScrollView::RemoveItem(int idx) {
    if (m_vecItems.empty()) return;
    if (idx < 0) {
        idx = (int)m_vecItems.size() - 1; // 마지막 아이템 제거
    }
    if (idx >= 0 && idx < (int)m_vecItems.size()) {
        CPathItem* pItem = m_vecItems[idx];
        if (pItem) {
            pItem->DestroyWindow();
            delete pItem;
        }
        m_vecItems.erase(m_vecItems.begin() + idx);
        CRect itemRect;
        // 아이템 제거 후 나머지 아이템들의 위치 재조정
        for (size_t i = 0; i < m_vecItems.size(); ++i) {
            if (m_vecItems[i] && m_vecItems[i]->GetSafeHwnd()) {
                
                m_vecItems[i]->GetWindowRect(&itemRect);
                int nItemHeight = itemRect.Height();
                m_vecItems[i]->SetWindowPos(NULL, 0, (int)i * nItemHeight, itemRect.Width(), nItemHeight, SWP_NOZORDER);
            }
        }
        // 스크롤 영역 재설정
        CRect viewRect;
        GetClientRect(&viewRect);
        CSize sizeTotal(viewRect.Width(), (int)m_vecItems.size() * itemRect.Height());
        SetScrollSizes(MM_TEXT, sizeTotal);
    }
}