#include <bits/stdc++.h>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) {
        val = x;
        next = nullptr;
    }
};

ListNode* reverse(ListNode* head) {
    // ListNode h = nullptr;
    ListNode* cur = head;
    ListNode* h = nullptr;
    while (cur != nullptr) {
        ListNode* nxt = cur->next;
        cur->next = h;
        h = cur;
        cur = nxt;
    }
    return h;
}

int main() {
    int k;
    cin >> k;
    ListNode preHead = ListNode(-1);
    int n;
    cin >> n;
    ListNode* cur = &preHead;
    for (int i = 0;i < n;i ++) {
        int x;
        cin >> x;
        cur->next = new ListNode(x);
        cur = cur->next;
    }
    cur = &preHead;
    ListNode* nxt = cur->next;
    while (cur != nullptr) {
        ListNode* record = cur;
        int cnt = 0;
        for (int i = 0;i < k;i ++) {
            if (nxt == nullptr) {
                break;
            }
            nxt = nxt->next;
            cur = cur->next;
            cnt ++;
        }
        if (cnt < k) {
            break;
        }
        cur->next = nullptr;
        ListNode* tmpHead = record->next;
        record->next = nullptr;
        ListNode* newHead = reverse(tmpHead);
        record->next = newHead;
        tmpHead->next = nxt;
        cur = tmpHead;
        nxt = cur->next;
    }
    ListNode* st = (&preHead)->next;
    while (st != nullptr) {
        printf("%d ", st->val);
        st = st->next;
    }
    return 0;
}