#include <iostream>

struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

void addNodeAtBeginning(ListNode*& head, int value) {
    ListNode* newNode = new ListNode(value);
    newNode->next = head;
    head = newNode;
}

void printList(ListNode* head) {
    while (head != nullptr) {
        std::cout << head->val << " ";
        head = head->next;
    }
    std::cout << std::endl;
}

void addNodeAtEnd(ListNode* head, int x){
    ListNode* newNode = new ListNode(x);
    while(head->next != nullptr){
        head = head->next; //链表前进
    }
    head->next = newNode;
}


int main1() {
    // 示例链表
    ListNode* head = new ListNode(1);
    head->next = new ListNode(2);
    head->next->next = new ListNode(3);

    std::cout << "原始链表：";
    printList(head);

    // 在链表开头添加新节点
    addNodeAtBeginning(head, 0);

    std::cout << "开头添加新节点后的链表：";
    printList(head);

    // 在链表末尾添加新节点
    addNodeAtEnd(head, 4);
    std::cout << "尾部添加新节点后的链表：";
    printList(head);

    return 0;
}

int main(){
    // 输入：head = [1,4,3,2,5,2], x = 3
    // 输出：[1,2,2,4,3,5]
    ListNode* head = new ListNode(1);
    addNodeAtEnd(head, 4);
    addNodeAtEnd(head, 3);
    addNodeAtEnd(head, 2);
    addNodeAtEnd(head, 5);
    addNodeAtEnd(head, 2);
    printList(head);
    
    int x = 3;
    ListNode* p1 = new ListNode(0);
    ListNode* p2 = new ListNode(0);
    ListNode* p = head;
    printList(p);

    while(p != nullptr){        
        if(p->val < x){
            addNodeAtEnd(p1, p->val);
        }else{
            addNodeAtEnd(p2, p->val);
        }
        p = p->next;
    }
    printList(p1);
    printList(p2);
    printList(p1->next);
    printList(p2->next);
    p1 = p1->next;
    p2 = p2->next;
    std::cout << "p1 p2" << std::endl;
    printList(p1);
    printList(p2);
    // p1 指针移到末尾(需要复制一个pp)
    ListNode* pp = p1;
    while(pp != nullptr && pp->next != nullptr){
        pp = pp->next;
    }
    pp->next = p2;
    printList(p1);
}

