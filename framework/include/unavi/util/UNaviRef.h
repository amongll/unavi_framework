/** \brief 
 * UNaviRefable.h
 *  Created on: 2014-9-9
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVIREF_H_
#define UNAVIREF_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

template <class RefType>
class UNaviRef
{
public:
	UNaviRef():obj(NULL),ref(NULL) {
	}

	UNaviRef(RefType& in):obj(&in),ref(new int(1)) {
	}

	UNaviRef(const UNaviRef& rh):obj(rh.obj),ref(rh.ref) {
		if(ref)(*(rh.ref))++;
	}

	~UNaviRef() {
		if (ref) {
			if (*ref == 1) {
				delete ref;
				delete obj;
			}
			else {
				(*ref)--;
			}
		}
	}

	UNaviRef& operator=(const UNaviRef& rh) {
		UNaviRef tmp(rh);
		swap(tmp);
		return *this;
	}

	void swap(UNaviRef& rh) {
		RefType* tmp_obj = rh.obj;
		rh.obj = obj;
		obj = tmp_obj;

		int* tmp_ref = rh.ref;
		rh.ref = ref;
		ref = tmp_ref;
	}

	RefType* get()const {
		return obj;
	}

	const RefType& operator*()const {
		return *obj;
	}

	const RefType* operator->()const {
		return obj;
	}

	RefType& operator*() {
		return *obj;
	}

	RefType* operator->() {
		return obj;
	}

	int use_count()const {
		return ref?*ref:0;
	}

	bool unique()const {
		return ref?*ref == 1:false;
	}

	operator bool()const {
		return obj != NULL;
	}

	void reset(RefType* in=NULL) {
		if (ref) {
			(*ref)--;
			if ((*ref)==0) {
				delete this->obj;
				delete ref;
				ref = NULL;
				obj = NULL;
			}
		}
		if (in) {
			this->obj = in;
			this->ref = new int(1);
		}
	}

	void reset(RefType& obj) {
		reset(&obj);
	}

	void unref() {
		if (ref) {
			(*ref)--;
			if ((*ref)==0) {
				delete ref;
			}
			ref = NULL;
			obj = NULL;
		}
	}
private:
	RefType *obj;
	int *ref;
};

UNAVI_NMSP_END

#endif /* UNAVIREFABLE_H_ */
