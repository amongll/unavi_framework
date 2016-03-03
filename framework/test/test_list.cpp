/*
 * test_list.cpp
 *
 *  Created on: 2014-9-30
 *      Author: dell
 */

#include "unavi/util/UNaviListable.h"

using namespace std;
using namespace unavi;

class TestNode : public UNaviListable
{
public:
	TestNode(int _a):a(_a)
	{

	}
	int a;
};

int main()
{
	UNaviList la,lb;
	for (int i=0; i<100; i++)
	{
		la.insert_tail(*(new TestNode(i)));
	}

	lb.take(la);

	for (int i=0; i<100; i++)
	{
		la.insert_tail(*(new TestNode(i)));
	}

	lb.take(la);

	int i = 50;
	while(i--) {
		TestNode* node = dynamic_cast<TestNode*>(lb.get_head());
		delete node;
	}

	TestNode* node = dynamic_cast<TestNode*>(lb.get_head());
	if (node) {
		cout<<node->a<<endl;
	}

	while(!lb.empty()) {
		TestNode* node = dynamic_cast<TestNode*>(lb.get_head());
		delete node;
	}
	return 0;
}


