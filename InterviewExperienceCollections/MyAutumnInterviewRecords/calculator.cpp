#include <bits/stdc++.h>
using namespace std;

using ll = long long;

/*
扑克牌实现24点。。。
*/

class Solution {
  public:
    /**
     * 代码中的类名、方法名、参数名已经指定，请勿修改，直接返回方法规定的值即可
     *
     *
     * @param s string字符串
     * @return int整型
     */
    int calculate(string s) {
        vector<int> weight(1000, 0);
        weight['+'] = 1;
        weight['-'] = 1;
        weight['*'] = 2;
        weight['/'] = 2;
        // write code here
        stack<ll> num;
        stack<char> ch;
        int sz = s.size();

        for (int i = 0; i < sz; i ++) {
            char c = s[i];
            if (c == '(') {
                ch.push(c);
            } else if (c >= '0' && c <= '9') {
                int j = i;
                ll t = 0;
                while (j < sz && (s[j] >= '0' && s[j] <= '9')) {
                    t = t * 10 + (s[j] - '0');
                    j ++;
                }
                i = j - 1;
                num.push(t);
            } else if (weight[c] > 0) {
                while (ch.size() > 0 && ch.top() != '(' && weight[ch.top()] >= weight[c]) {
                    funcCal(num, ch);
                }
                ch.push(c);
            } else if (c == ')') {
                while (ch.top() != '(') {
                    funcCal(num, ch);
                }
                ch.pop();
            }
        }
        while (!ch.empty()) {
            funcCal(num, ch);
        }
        return num.top();
    }
    int funcCal(stack<ll>& num, stack<char>& ch) {
        char cur_ch = ch.top();

        ch.pop();
        ll numB = num.size() > 0 ? num.top() : 0;
        if (num.size() > 0) {
            num.pop();
        }
        ll numA = num.size() > 0 ? num.top() : 0;
        if (num.size() > 0) {
            num.pop();
        }
        ll val = -1;
        switch (cur_ch) {
            case '+':
                val = numA + numB;
                num.push(val);
                break;
            case '-':
                val = numA - numB;
                num.push(val);
                break;
            case '*':
                num.push(numA * numB);
                break;
            case '/':
                num.push(numA / numB);
                break;
            default:
                printf("stange error");
        }
        printf("%lld\n", num.top());
        return -1;
    }
};

/*

*/