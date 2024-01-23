#include <iostream>
using namespace std;

void test_array(){
    int array[2][3] = {
        {0,1,2},
        {3,4,5},
    };
    cout << &array[0][0] <<" "<< &array[0][1] <<" "<< &array[0][2] <<" "<< endl;
    cout << &array[1][0] <<" "<< &array[1][1] <<" "<< &array[1][2] <<" "<< endl;
}

int main(){
    test_array();
    return 0;
}

// 0x7ffd8618ab30 0x7ffd8618ab34 0x7ffd8618ab38 
// 0x7ffd8618ab3c 0x7ffd8618ab40 0x7ffd8618ab44