/*-----------------------------------------------------------------------------
/ Title      : iMS Containers
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/Containers.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2016-10-01
/ Last update: $Date: 2020-06-05 07:45:07 +0100 (Fri, 05 Jun 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 443 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2016 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2016-10-01  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "Containers.h"
#include "Image.h"
#include "Compensation.h"
#include "ToneBuffer.h"

// Required for UUID
#if defined(__QNXNTO__)
#include <process.h>
#include <time.h>
#endif
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace iMS {

	template <typename T>
	class ListBase<T>::ListImpl
	{
	public:
		ListImpl() : tag(boost::uuids::random_generator()()), m_modified_time(0), tagDirty(false) {}

		// Update the UUID whenever the image has been modified
		void updateUUID() {
			//this->tag = boost::uuids::random_generator()();
			tagDirty = true;
			time(&this->m_modified_time);
		}
		void refreshTag() {
		  if (tagDirty) {
		    this->tag = boost::uuids::random_generator()();
		    tagDirty = false;
		  }
		}

		bool tagDirty;
		std::list<T> m_list;
		std::string m_name;
		boost::uuids::uuid tag;
		std::time_t m_modified_time;
	};

	template <typename T>
	ListBase<T>::ListBase(const std::string& Name, const std::time_t& modified_time)
		: p_ListImpl(new ListImpl()) { 
		p_ListImpl->m_name = Name;
		p_ListImpl->m_modified_time = modified_time;
	}

	template <typename T>
	ListBase<T>::~ListBase() { delete p_ListImpl; p_ListImpl = nullptr; }

	template <typename T>
	ListBase<T>::ListBase(const ListBase<T> &rhs) : p_ListImpl(new ListImpl())
	{
		p_ListImpl->m_list = rhs.p_ListImpl->m_list;
		p_ListImpl->m_name = rhs.p_ListImpl->m_name;
		p_ListImpl->tag = rhs.p_ListImpl->tag;
		p_ListImpl->m_modified_time = rhs.p_ListImpl->m_modified_time;
	}

	template <typename T>
	ListBase<T> &ListBase<T>::operator = (const ListBase<T> &rhs)
	{
		if (this == &rhs) return *this;
		p_ListImpl->m_list = rhs.p_ListImpl->m_list;
		p_ListImpl->m_name = rhs.p_ListImpl->m_name;
		p_ListImpl->tag = rhs.p_ListImpl->tag;
		p_ListImpl->m_modified_time = rhs.p_ListImpl->m_modified_time;
		return *this;
	}

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::begin() { return p_ListImpl->m_list.begin(); }

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::end() { return p_ListImpl->m_list.end(); }

	template <typename T>
	typename ListBase<T>::const_iterator ListBase<T>::begin() const { return p_ListImpl->m_list.cbegin(); }

	template <typename T>
	typename ListBase<T>::const_iterator ListBase<T>::end() const { return p_ListImpl->m_list.cend(); }

	template <typename T>
	typename ListBase<T>::const_iterator ListBase<T>::cbegin() const { return begin(); }

	template <typename T>
	typename ListBase<T>::const_iterator ListBase<T>::cend() const { return end(); }


	template <typename T>
	bool ListBase<T>::operator==(ListBase<T> const& rhs) const {
		if (p_ListImpl->tagDirty) return false;
		return p_ListImpl->tag == rhs.p_ListImpl->tag;
	}

	template <typename T>
	bool ListBase<T>::empty() const { return p_ListImpl->m_list.empty(); }

	template <typename T>
	std::size_t ListBase<T>::size() const { return p_ListImpl->m_list.size(); }

	template <typename T>
	void ListBase<T>::assign(size_t n, const T& val) {
		p_ListImpl->m_list.assign(n, val);
		this->p_ListImpl->updateUUID();
	}

	template <typename T>
	void ListBase<T>::push_front(const T& val) {
		p_ListImpl->m_list.push_front(val);
		this->p_ListImpl->updateUUID();
	}

	template <typename T>
	void ListBase<T>::pop_front() {
		p_ListImpl->m_list.pop_front();
		this->p_ListImpl->updateUUID();
	}

	template <typename T>
	void ListBase<T>::push_back(const T& val) {
		p_ListImpl->m_list.push_back(val);
		this->p_ListImpl->updateUUID();
	}

	template <typename T>
	void ListBase<T>::pop_back() {
		p_ListImpl->m_list.pop_back();
		this->p_ListImpl->updateUUID();
	}

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::insert(iterator position, const T& val) {
		this->p_ListImpl->updateUUID();
		return p_ListImpl->m_list.insert(position, val);
	}

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::insert(iterator position, const_iterator first, const_iterator last) {
		this->p_ListImpl->updateUUID();
		return p_ListImpl->m_list.insert(position, first, last);
	}

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::erase(iterator position) {
		this->p_ListImpl->updateUUID();
		return p_ListImpl->m_list.erase(position);
	}

	template <typename T>
	typename ListBase<T>::iterator ListBase<T>::erase(iterator first, iterator last)  {
		this->p_ListImpl->updateUUID();
		return p_ListImpl->m_list.erase(first, last);
	}

	template <typename T>
	void ListBase<T>::resize(size_t n) {
		if (n != this->size()) this->p_ListImpl->updateUUID();
		p_ListImpl->m_list.resize(n);
	}

	template <typename T>
	void ListBase<T>::clear() {
		this->p_ListImpl->updateUUID();
		p_ListImpl->m_list.clear();
	}

	template <typename T>
	const std::array<std::uint8_t, 16> ListBase<T>::GetUUID() const
	{
		p_ListImpl->refreshTag();
		boost::uuids::uuid u = p_ListImpl->tag;
		std::array<std::uint8_t, 16> v;
		std::copy_n(u.begin(), 16, v.begin());
		return v;
	}

	template <typename T>
	const std::time_t& ListBase<T>::ModifiedTime() const
	{
		return p_ListImpl->m_modified_time;
	}

	template <typename T>
	std::string ListBase<T>::ModifiedTimeFormat() const
	{
		char buffer[80];
		std::strftime(buffer, sizeof(buffer), "%c %Z", std::localtime(&p_ListImpl->m_modified_time));
		return std::string(buffer);
	}

	template <typename T>
	const std::string& ListBase<T>::Name() const
	{
		return p_ListImpl->m_name;
	}

	template <typename T>
	std::string& ListBase<T>::Name()
	{
		return p_ListImpl->m_name;
	}


	/// \cond TEMPLATE_INSTANTIATIONS
	// Explicitly instantiate ListBase class so code is compiled and added to the library (which is then inherited by other specialised classes)
	template class ListBase < std::shared_ptr < SequenceEntry > >;
	template class ListBase < ImageGroup >;
	template class ListBase < CompensationFunction >;
	template class ListBase < ToneBuffer >;
	template class ListBase < CompensationPointSpecification >;
	template class ListBase < std::string >;
	/// \endcond

	template <typename T>
	class DequeBase<T>::DequeImpl
	{
	public:
		DequeImpl() : tag(boost::uuids::random_generator()()), m_modified_time(0), tagDirty(false) {}
		DequeImpl(std::size_t n, const T& v) : tag(boost::uuids::random_generator()()), m_modified_time(0), tagDirty(false) { m_deque.assign(n, v); }
		DequeImpl(const_iterator first, const_iterator last) : tag(boost::uuids::random_generator()()), m_modified_time(0), tagDirty(false) { m_deque.assign(first, last); }

		// Update the UUID whenever the deque has been modified
		void updateUUID() {
			//this->tag = boost::uuids::random_generator()();
			tagDirty = true;
			time(&this->m_modified_time);
		}
		void refreshTag() {
		  if (tagDirty) {
		    this->tag = boost::uuids::random_generator()();
		    tagDirty = false;
		  }
		}

		bool tagDirty;
		std::deque<T> m_deque;
		std::string m_name;
		boost::uuids::uuid tag;
		std::time_t m_modified_time;
	};


	template <typename T>
	DequeBase<T>::DequeBase(const std::string& Name, const std::time_t& modified_time)
		: p_DequeImpl(new DequeImpl()) {
		p_DequeImpl->m_name = Name; 
		p_DequeImpl->m_modified_time = modified_time; 
	}

	template <typename T>
	DequeBase<T>::~DequeBase() { delete p_DequeImpl; p_DequeImpl = nullptr; }

	template <typename T>
	DequeBase<T>::DequeBase(size_t n, const T& v, const std::string& Name, const std::time_t& modified_time)
		: p_DequeImpl(new DequeImpl(n, v)) {
		p_DequeImpl->m_name = Name;
		p_DequeImpl->m_modified_time = modified_time;
	}

	template <typename T>
	DequeBase<T>::DequeBase(const_iterator first, const_iterator last, const std::string& Name, const std::time_t& modified_time) 
		: p_DequeImpl(new DequeImpl(first, last)) {
		p_DequeImpl->m_name = Name;
		p_DequeImpl->m_modified_time = modified_time;
	}

	template <typename T>
	DequeBase<T>::DequeBase(const DequeBase<T> &rhs) : p_DequeImpl(new DequeImpl())
	{
		p_DequeImpl->m_deque = rhs.p_DequeImpl->m_deque;
		p_DequeImpl->m_name = rhs.p_DequeImpl->m_name;
		p_DequeImpl->tag = rhs.p_DequeImpl->tag;
		p_DequeImpl->m_modified_time = rhs.p_DequeImpl->m_modified_time;
	}

	template <typename T>
	DequeBase<T> &DequeBase<T>::operator =(const DequeBase<T> &rhs) {
		if (this == &rhs) return *this;
		p_DequeImpl->m_deque = rhs.p_DequeImpl->m_deque;
		p_DequeImpl->m_name = rhs.p_DequeImpl->m_name;
		p_DequeImpl->tag = rhs.p_DequeImpl->tag;
		p_DequeImpl->m_modified_time = rhs.p_DequeImpl->m_modified_time;
		return *this;
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::begin() { return p_DequeImpl->m_deque.begin(); }

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::end() { return p_DequeImpl->m_deque.end(); }

	template <typename T>
	typename DequeBase<T>::const_iterator DequeBase<T>::begin() const { return p_DequeImpl->m_deque.cbegin(); }

	template <typename T>
	typename DequeBase<T>::const_iterator DequeBase<T>::end() const { return p_DequeImpl->m_deque.cend(); }

	template <typename T>
	typename DequeBase<T>::const_iterator DequeBase<T>::cbegin() const { return begin(); }

	template <typename T>
	typename DequeBase<T>::const_iterator DequeBase<T>::cend() const { return end(); }

	template <typename T>
	bool DequeBase<T>::operator==(DequeBase<T> const& rhs) const {
		if (p_DequeImpl->tagDirty) return false;
		return p_DequeImpl->tag == rhs.p_DequeImpl->tag;
	}

	template <typename T>
	T& DequeBase<T>::operator[](int idx) { 
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.at(idx);
	}

	template <typename T>
	const T& DequeBase<T>::operator[](int idx) const { return p_DequeImpl->m_deque.at(idx); }

	template <typename T>
	const std::array<std::uint8_t, 16> DequeBase<T>::GetUUID() const	{
		p_DequeImpl->refreshTag();
		boost::uuids::uuid u = p_DequeImpl->tag;
		std::array<std::uint8_t, 16> v;
		std::copy_n(u.begin(), 16, v.begin());
		return v;
	}

	template <typename T>
	const std::time_t& DequeBase<T>::ModifiedTime() const
	{
		return p_DequeImpl->m_modified_time;
	}

	template <typename T>
	std::string DequeBase<T>::ModifiedTimeFormat() const
	{
		char buffer[80];
		std::strftime(buffer, sizeof(buffer), "%c %Z", std::localtime(&p_DequeImpl->m_modified_time));
		return std::string(buffer);
	}

	template <typename T>
	void DequeBase<T>::clear() { 
		this->p_DequeImpl->updateUUID();
		p_DequeImpl->m_deque.clear();
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::insert(iterator pos, const T& value) {
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.insert(pos, value);
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::insert(const_iterator pos, size_t count, const T& value) {
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.insert(pos, count, value);
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::insert(iterator pos, const_iterator first, const_iterator last) {
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.insert(pos, first, last);
	}

	template <typename T>
	void DequeBase<T>::push_back(const T& value) {
		this->p_DequeImpl->updateUUID();
		p_DequeImpl->m_deque.push_back(value);
	}

	template <typename T>
	void DequeBase<T>::pop_back() {
		this->p_DequeImpl->updateUUID();
		p_DequeImpl->m_deque.pop_back();
	}

	template <typename T>
	void DequeBase<T>::push_front(const T& value) {
		this->p_DequeImpl->updateUUID();
		p_DequeImpl->m_deque.push_front(value);
	}

	template <typename T>
	void DequeBase<T>::pop_front() {
		this->p_DequeImpl->updateUUID();
		p_DequeImpl->m_deque.pop_front();
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::erase(iterator pos) {
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.erase(pos);
	}

	template <typename T>
	typename DequeBase<T>::iterator DequeBase<T>::erase(iterator first, iterator last) {
		this->p_DequeImpl->updateUUID();
		return p_DequeImpl->m_deque.erase(first, last);
	}

	template <typename T>
	const std::string& DequeBase<T>::Name() const
	{
		return p_DequeImpl->m_name;
	}

	template <typename T>
	std::string& DequeBase<T>::Name()
	{
		return p_DequeImpl->m_name;
	}

	template <typename T>
	std::size_t DequeBase<T>::size() const { return p_DequeImpl->m_deque.size(); }

	/// \cond TEMPLATE_INSTANTIATIONS
	// Explicitly instantiate DequeBase class so code is compiled and added to the library (which is then inherited by other specialised classes)
	template class DequeBase < ImagePoint >;
	template class DequeBase < Image >;
	template class DequeBase < CompensationPoint > ;
	template class DequeBase < CompensationTable > ;
	/// \endcond


}
