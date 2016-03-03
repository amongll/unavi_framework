/** \brief 
 * UNaviQuitable.h
 *  Created on: 2014-9-9
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVILISTABLE_H_
#define UNAVILISTABLE_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN
class UNaviList;
class UNaviListable
{
public:
	UNaviListable() {
		next = prev = this;
	}

	virtual ~UNaviListable() {
	    next->prev = prev;
	    prev->next = next;
	}

	bool empty()const {
		return next == this;
	}

	void quit_list() {
		next->prev = prev;
		prev->next = next;
		prev = next = this;
	}

	UNaviListable *next;
	UNaviListable *prev;
private:
	UNaviListable(const UNaviListable&);
};

#define unavi_list_data(l, type, link)                                        \
    (type *) ((u_char *) l - offsetof(type, link))


class UNaviListIter;
class UNaviList
{
public:
	class Iterator;
	UNaviList():
		recycled_iter_cnt(0)
	{}

	~UNaviList();
	UNaviListable senti;
	bool empty()const {
		return senti.empty();
	}
	UNaviListable* get_head() {
		if (empty()) return NULL;
		return senti.next;
	}
	UNaviListable* get_tail() {
		if (empty()) return NULL;
		return senti.prev;
	}
	UNaviListable* get_next(UNaviListable* cur) 
	{
		if ( cur->next == &senti )
			return NULL;
		return cur->next;
	}
	UNaviListable* get_prev(UNaviListable* cur) 
	{
		if ( cur->prev == &senti)
			return NULL;
		return cur->prev;
	}

	void quit_node(UNaviListable* nd);

	void insert_after(UNaviListable& position, UNaviListable& new_node)
	{
		if (&position == &new_node) return;
		new_node.quit_list();
		if (&position == &senti) return;
		new_node.next = position.next;
		new_node.prev = &position;
		position.next->prev = &new_node;
		position.next = &new_node;
	}

	void insert_before(UNaviListable& position, UNaviListable& new_node)
	{
		if (&position == &new_node) return;
		new_node.quit_list();
		if (&position == &senti) return;
		new_node.next = &position;
		new_node.prev = position.prev;
		position.prev->next = &new_node;
		position.prev = &new_node;
	}

	void insert_head(UNaviListable& entry) {
		entry.quit_list();
		entry.next = senti.next;
		entry.prev = &senti;\
		senti.next->prev = &entry;
		senti.next = &entry;
	}
	void insert_tail(UNaviListable& entry) {
		entry.quit_list();
	    entry.prev = senti.prev; \
	    entry.next = &senti; \
	    senti.prev->next = &entry; \
	    senti.prev = &entry;\
	}
	void swap(UNaviList& rh);
	void take(UNaviList& rh);
	void giveto(UNaviList& dst);

	void print_list(char a);
	UNaviList(const UNaviList& rh){}

	void recycle_all_nodes();
private:
	UNaviList& operator=(const UNaviList&);

	class IterImpl :public UNaviListable {
	friend class UNaviList;
	friend class Iterator;
		IterImpl():target(NULL),user(NULL){}
		UNaviListable* target;
		Iterator* user;
	};

	mutable UNaviListable using_iters; // IterImpl½Úµã
	mutable UNaviListable recycled_iters;
	mutable uint8_t recycled_iter_cnt;
public:
	class Iterator
	{
	friend class UNaviList;
		IterImpl* impl;
		UNaviList* lst;
		mutable bool swallow;
		Iterator(UNaviList* l=NULL):lst(l),impl(NULL),swallow(false){}
	public:
		~Iterator();
		Iterator(const Iterator&);
		Iterator& operator=(const Iterator&);
		bool operator==(const Iterator& rh)const;
		Iterator& operator++();
		UNaviListable* operator*();
		const UNaviListable* operator*()const;

	};
	friend class Iterator;

	IterImpl* get_iter_node()const ;

	void recycle_iter_node(IterImpl*)const;

	typedef Iterator iterator;

	iterator begin();
	iterator end()
	{
		return Iterator(this);
	}
};

UNAVI_NMSP_END

#endif /* UNAVIQUITABLE_H_ */
