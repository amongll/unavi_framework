/*
 * UNaviListable.cpp
 *
 *  Created on: 2014-9-24
 *      Author: lilei
 *
 */

#include "unavi/util/UNaviListable.h"

using namespace std;
using namespace unavi;

void  UNaviList::swap(UNaviList& rh) {
	if (rh.empty() && empty()) {
		return;
	}
	else if (rh.empty()) {
		senti.next->prev = &rh.senti;
		senti.prev->next = &rh.senti;
		rh.senti.next = senti.next;
		rh.senti.prev = senti.prev;
		senti.next = senti.prev = &senti;
	}
	else if (empty()) {
		rh.senti.next->prev = &senti;
		rh.senti.prev->next = &senti;
		senti.next = rh.senti.next;
		senti.prev = rh.senti.prev;
		rh.senti.next = rh.senti.prev = &rh.senti;
	}
	else {
		UNaviListable* tmp_a = senti.next;
		UNaviListable* tmp_b = rh.senti.next;
		senti.next = tmp_b;
		tmp_b->prev = &senti;
		rh.senti.next = tmp_a;
		tmp_a->prev = &rh.senti;

		tmp_a = senti.prev;
		tmp_b = rh.senti.prev;
		senti.prev = tmp_b;
		tmp_b->next = &senti;
		rh.senti.prev = tmp_a;
		tmp_a->next = &rh.senti;
	}
}

void  UNaviList::take(UNaviList& rh)
{
	if (rh.empty())return;
	senti.prev->next = rh.senti.next;
	rh.senti.next->prev = senti.prev;
	senti.prev = rh.senti.prev;
	senti.prev->next = &senti;
	rh.senti.next = rh.senti.prev = &rh.senti;
}

void  UNaviList::giveto(UNaviList& rh)
{
	if (empty()) return;
	rh.senti.prev->next = senti.next;
	senti.next->prev = rh.senti.prev;
	rh.senti.prev = senti.prev;
	rh.senti.prev->next = &rh.senti;
	senti.next = senti.prev = &senti;
}

void UNaviList::print_list(char a)
{
	UNaviListable* link = senti.next;
	printf("%c senti:%p[%p %p] ",a, &senti,senti.next,senti.prev);
	while( link != &senti ) {
		printf("%p[%p,%p]",link,link->next,link->prev);
		link = link->next;
	}
	printf("\n");
}

UNaviList::~UNaviList()
{
	while (!using_iters.empty()) {
		UNaviListable* nd = using_iters.next;
		delete nd;
	}
	while (!recycled_iters.empty()) {
		UNaviListable* nd  = recycled_iters.next;
		delete nd;
	}
}

void UNaviList::quit_node(UNaviListable* nd)
{
	UNaviListable* ck = using_iters.next;
	while( ck != &using_iters ) {
		IterImpl* it = dynamic_cast<IterImpl*>(ck);
		ck = it->next;

		if (it->target == nd) {
			it->target = nd->next;

			if (it->target == &senti ) {
				if (it->user) {
					it->user->impl = NULL;
				}
				recycle_iter_node(it);
			}
		}
	}
	nd->quit_list();
}

UNaviList::Iterator::Iterator(const Iterator& rh):
	lst(rh.lst),
	impl(NULL),
	swallow(false)
{
	if ( lst && rh.impl ) {
		if (rh.swallow) {
			Iterator& _rh = const_cast<Iterator&>(rh);
			impl = rh.impl;
			impl->user = this;
			_rh.impl = NULL;
			return;
		}
		else {
			impl = lst->get_iter_node();
			impl->user = this;
			impl->target = rh.impl->target;
			return;
		}
	}
}

UNaviList::Iterator::~Iterator()
{
	if (lst && impl) {
		lst->recycle_iter_node(impl);
	}
}

UNaviList::Iterator& UNaviList::Iterator::operator=(const Iterator& rh)
{
	if (lst&&impl) {
		lst->recycle_iter_node(impl);
		lst = NULL;
		impl = NULL;
	}

	lst = rh.lst;
	if (lst&&rh.impl) {
		if(rh.swallow) {
			Iterator& _rh = const_cast<Iterator&>(rh);
			impl = rh.impl;
			impl->user = this;
			_rh.impl = NULL;
		}
		else {
			impl = lst->get_iter_node();
			impl->user = this;
			impl->target = rh.impl->target;
		}
	}

	return *this;
}

bool UNaviList::Iterator::operator==(const Iterator& rh)const
{
	if (lst == rh.lst) {
		if (impl == rh.impl)
			return true;

		if ( impl == NULL || rh.impl == NULL ) {
			return false;
		}

		if ( impl->target == rh.impl->target )
			return true;
	}
	return false;
}

UNaviList::Iterator& UNaviList::Iterator::operator++()
{
	if (swallow)swallow=false;

	if (!lst) return *this;
	if (!impl)return *this;

	if ( impl->target->next == &lst->senti ) {
		lst->recycle_iter_node(impl);
		impl = NULL;
		return *this;
	}

	impl->target = impl->target->next;
	return *this;
}

UNaviListable* UNaviList::Iterator::operator *()
{
	if (swallow)swallow=false;
	if (!lst || !impl) return NULL;
	return impl->target;
}

const UNaviListable* UNaviList::Iterator::operator*()const
{
	if (swallow)swallow=false;
	if (!lst || !impl) return NULL;
	return impl->target;
}

UNaviList::Iterator UNaviList::begin()
{
	if ( empty() ) {
		return Iterator(this);
	}

	Iterator ret(this);
	ret.swallow = true;
	ret.impl = get_iter_node();
	ret.impl->target = senti.next;
	return ret;
}

UNaviList::IterImpl* UNaviList::get_iter_node()const
{
	IterImpl* nd = NULL;
	if ( !recycled_iters.empty() ) {
		nd = dynamic_cast<IterImpl*>(recycled_iters.next);
		nd->next->prev = nd->prev;
		nd->prev->next = nd->next;
	}
	else {
		nd = new IterImpl;
	}

	nd->next = using_iters.next;
	nd->prev = &using_iters;
	using_iters.next->prev = nd;
	using_iters.next = nd;

	return nd;
}

void UNaviList::recycle_iter_node(IterImpl* nd)const
{
	if ( recycled_iter_cnt > 10) {
		delete nd;
		return;
	}

	nd->target = NULL;
	nd->user = NULL;

	nd->next->prev = nd->prev;
	nd->prev->next = nd->next;

	nd->next = recycled_iters.next;
	nd->prev = &recycled_iters;
	recycled_iters.next->prev =nd;
	recycled_iters.next = nd;
	recycled_iter_cnt++;
}

void UNaviList::recycle_all_nodes()
{
	UNaviListable* nd = get_head();
	while(nd) {
		quit_node(nd);
		delete nd;
		nd = get_head();
	}
}
