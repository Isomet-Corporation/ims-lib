/*-----------------------------------------------------------------------------
/ Title      : iMS Containers
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/Containers.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2016-10-01
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2016 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file Containers.h
///
/// \brief Container Classes for storing various types of data related to Image classes and others
///
/// \author Dave Cowan
/// \date 2016-10-01
/// \since 1.3
/// \ingroup group_Containers
///

#ifndef IMS_CONTAINERS_H__
#define IMS_CONTAINERS_H__

#include <deque>
#include <list>
#include <array>
#include <cstdint>
#include <ctime>
#include <string>

/// \cond LIB_CREATION
#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define DLL_EXPORT __attribute__ ((dllexport))
    #define DLL_IMPORT __attribute__ ((dllimport))
  #else
    #define DLL_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #define DLL_IMPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define DLL_EXPORT __attribute__ ((visibility ("default")))
    #define DLL_IMPORT __attribute__ ((visibility ("default")))
    #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define DLL_EXPORT
    #define DLL_IMPORT
    #define DLL_LOCAL
  #endif
#endif

#if defined(_EXPORTING_IMS)
  #define LIBSPEC DLL_EXPORT
  #define LIBLOCAL DLL_LOCAL
  #define EXPIMP_TEMPLATE
#elif defined(_STATIC_IMS)
  #define LIBSPEC
  #define LIBLOCAL
  #define EXPIMP_TEMPLATE
#else
  #define LIBSPEC DLL_IMPORT
  #define LIBLOCAL DLL_LOCAL
  #define EXPIMP_TEMPLATE extern
#endif
/// \endcond

namespace iMS {

	///
	/// \class ListBase Containers.h include/Containers.h
	/// \brief Template Class encapsulating a list object and acting as a base list class for other classes in the library to inherit from
	///
	/// \date 2016-11-09
	/// \since 1.3
	///
	template <typename T>
	class LIBSPEC ListBase
	{
	public:
		/// \name Constructors & Destructor
		//@{
		/// \brief Create a default empty List with optional name parameter
		ListBase(const std::string& Name = "[no name]", const std::time_t& modified_time = std::time(nullptr));
		/// \brief Destructor
		~ListBase();
		/// \brief Copy Constructor
		ListBase(const ListBase &);
		/// \brief Assignment Constructor
		ListBase &operator =(const ListBase &);
		//@}

		/// \name Iterator Specification
		///
		/// Use these iterators when you want to iteratively read through or update the entries stored
		/// within a ListBase.  Iterators can be used to access elements at an arbitrary offset position
		/// relative to the element they point to.
		///
		/// Two types of iterators are supported; both are random access iterators.  Dereferencing const_iterator
		/// yields a reference to a constant entry in the ListBase(const ListBase&).
		///
		//@{
		/// \brief Iterator defined for user manipulation of ListBase
		typedef typename std::list<T>::iterator iterator;
		/// \brief Const Iterator defined for user readback of ListBase
		typedef typename std::list<T>::const_iterator const_iterator;
		/// \brief Returns an iterator pointing to the first element in the ListBase container.
		/// \return An iterator to the beginning of the ListBase container.
		iterator begin();
		/// \brief Returns an iterator referring to the past-the-end element in the ListBase container.
		///
		/// The past-the-end element is the theoretical element that would follow the last element
		/// in the ListBase container. It does not point to any element, and thus shall not be dereferenced.
		///
		/// Because the ranges used by functions of the standard library do not include the element
		/// pointed by their closing iterator, this function can be used in combination with
		/// ListBase::begin to specify a range including all the elements in the container.
		///
		/// \return An iterator to the element past the end of the ListBase
		iterator end();
		/// \brief Returns a const_iterator pointing to the first element in the ListBase container.
		/// \return A ListBase to the beginning of the ListBase container.
		/// \since 1.2.5
		const_iterator begin() const;
		/// \brief Returns a const_iterator referring to the past-the-end element in the ListBase container.
		/// \return A const_iterator to the element past the end of the ListBase.
		/// \since 1.2.5
		const_iterator end() const;
		/// \brief Returns a const_iterator pointing to the first element in the ListBase container.
		/// \return A const_iterator to the beginning of the ListBase container.
		const_iterator cbegin() const;
		/// \brief Returns a const_iterator referring to the past-the-end element in the ListBase container.
		/// \return A const_iterator to the element past the end of the ListBase.
		const_iterator cend() const;
		//@}

		/// \brief Equality Operator checks ListBase object for equivalence
		///
		/// \param[in] rhs An ListBase object to perform the comparison with
		/// \return True if the supplied ListBase is identical to this one.
		bool operator==(ListBase const& rhs) const;

		///
		/// \name ListBase Unique Identifier
		//@{
		///
		/// \brief Returns a vector representing the Unique Identifier assigned to the ListBase object
		/// \return UUID as an array of uint8_t's
		const std::array<std::uint8_t, 16> GetUUID() const;
		//@}
		///

		///
		/// \name Timestamping
		//@{
		///
		/// \brief Returns Time at which the Container was last modified
		///
		/// Any time the container is modified (added to, deleted from, elements updated), the system time is recorded.
		/// This happens coincident with the UUID if the container also being updated.  This function returns to the user that timestamp.
		/// \return a reference to a std::time_t representing the time at which the container was last modified
		/// \since 1.3
		const std::time_t& ModifiedTime() const;
		///
		/// \brief Returns Human-readable string for the time at which the Container was last modified
		/// \since 1.3
		std::string ModifiedTimeFormat() const;
		//@}
		///

		///
		/// \name Container Description
		//@{
		///
		/// \brief A string stored with the Container to aid human users in identifying its purpose 
		///
		/// Updating the Container Name does not cause the Container UUID to change.
		const std::string& Name() const;
		std::string& Name();
		//@}
		///

		/// \name Modifiers
		//@{
		/// \brief Assign new content to ImageSequence list
		///
		/// Assigns new contents to the ImageSequence list container, replacing its current contents, and modifying its size accordingly.
		/// the new contents are \c n elements, each initialized to a copy of \c val.
		/// \param[in] n New size for the container.
		/// \param[in] val ImageSequenceEntry to fill the ImageSequence with. Each of the n elements in the container will be initialized to a copy of this value.
		void assign(size_t n, const T& val);
		/// \brief Insert ImageSequenceEntry at beginning
		///
		/// Inserts a new element at the beginning of the list, right before its current first element.
		/// The content of val is copied (or moved) to the inserted element.
		/// This effectively increases the Sequence size by one.
		/// \param[in] val ImageSequenceEntry to be copied (or moved) to the inserted element.
		void push_front(const T& val);
		/// \brief Delete first ImageSequenceEntry
		///
		/// Removes the first ImageSequenceEntry in the ImageSequence list container, effectively reducing its size by one.
		/// This destroys the removed entry.
		void pop_front();
		/// \brief Add ImageSequenceEntry at end
		///
		/// Adds a new ImageSequenceEntry at the end of the ImageSequence list container, after its current last element.
		/// The content of val is copied (or moved) to the new element.
		/// This effectively increases the Sequence size by one.
		/// \param[in] val ImageSequenceEntry to be copied (or moved) to the new element.
		void push_back(const T& val);
		/// \brief Delete last ImageSequenceEntry
		///
		/// Removes the last ImageSequenceEntry in the ImageSequence list container, effectively reducing its size by one.
		/// This destroys the removed entry.
		void pop_back();
		/// \brief Insert ImageSequenceEntry
		///
		/// The ImageSequence container is extended by inserting new elements before the element at the specified position.
		/// This effectively increases the ImageSequence list size by one.
		/// \param[in] position Position in the container where the new elements are inserted.
		/// \param[in] val ImageSequenceEntry Value to be copied (or moved) to the inserted elements.
		/// \return An iterator that points to the first of the newly inserted elements.
		iterator insert(iterator position, const T& val);
		/// \brief Insert Range Of ImageSequenceEntry's
		///
		/// The ImageSequence container is extended by inserting new elements before the element at the specified position
		/// from a range of ImageSequenceEntries present in another ImageSequence.
		/// This effectively increases the ImageSequence list size by the number of entries in the range
		/// \param[in] position Position in the container where the new elements are inserted.
		/// \param[in] first Iterator specifying the first of a range of elements.
		/// \param[in] last Iterator specifying the last of a range of elements.  All the elements between first and last,
		/// including the element pointed by first but not the one pointed by last are inserted to the ImageSequence before position.
		/// \return An iterator that points to the first of the newly inserted elements.
		iterator insert(iterator position, const_iterator first, const_iterator last);
		/// \brief Erase ImageSequenceEntry
		///
		///  Removes a single ImageSequenceEntry element (at position) from the list container
		/// \param[in] position Iterator pointing to a single element to be removed from the list.
		/// \return An iterator pointing to the element that followed the last element erased by the function call.
		/// This is the container end if the operation erased the last element in the sequence.
		iterator erase(iterator position);
		/// \brief Erase a range of ImageSequenceEntry's
		///
		///  Removes a range of ImageSequenceEntry elements (first,last) from the list container
		/// \param[in] first Iterators within the list to be removed.
		/// \param[in] last Iterators within the list to be removed.
		/// \return An iterator pointing to the element that followed the last element erased by the function call.
		/// This is the container end if the operation erased the last element in the sequence.
		iterator erase(iterator first, iterator last);
		/// \brief Change Size
		///
		/// Resizes the ImageSequence container so that it contains n elements.
		/// If n is smaller than the current container size, the content is reduced to its first n elements, removing those beyond (and destroying them).
		/// If n is greater than the current container size, the content is expanded by inserting at the end as many elements as needed
		/// to reach a size of n. The new ImageSequenceEntry 's are default-initialized.
		/// Notice that this function changes the actual content of the container by inserting or erasing elements from it.
		/// \param[in] n New container size, expressed in number of elements.
		void resize(size_t n);
		/// \brief Clear Content
		///
		///  Removes all elements from the list container (which are destroyed), and leaving the ImageSequence with a size of 0.
		void clear();
		//@}

		/// \name Helper Functions
		//@{
		/// \brief Returns True if the ListBase is empty
		/// \return True if the ListBase is empty
		bool empty() const;
		/// \brief Returns the Number of Entries in the ListBase
		/// \return std::size_t representing the number of elements in the ListBase
		std::size_t size() const;
		//@}

	private:
		class ListImpl;
		ListImpl *p_ListImpl;
	};


	///
	/// \class DequeBase Containers.h include/Containers.h
	/// \brief Template Class encapsulating a deque object and acting as a base deque class for other classes in the library to inherit from
	///
	/// \date 2016-11-09
	/// \since 1.3
	///
	template <typename T>
	class LIBSPEC DequeBase
	{
	public:
		/// \name Iterator Specification
		///
		/// Use these iterators when you want to iteratively read through or update the entries stored
		/// within a DequeBase.  Iterators can be used to access elements at an arbitrary offset position
		/// relative to the element they point to.
		///
		/// Two types of iterators are supported; both are random access iterators.  Dereferencing const_iterator
		/// yields a reference to a constant entry in the DequeBase(const DequeBase&).
		///
		//@{
		/// \brief Iterator defined for user manipulation of DequeBase
		typedef typename std::deque<T>::iterator iterator;
		/// \brief Const Iterator defined for user readback of DequeBase
		typedef typename std::deque<T>::const_iterator const_iterator;
		/// \brief Returns an iterator pointing to the first element in the DequeBase container.
		/// \return An iterator to the beginning of the DequeBase container.
		iterator begin();
		/// \brief Returns an iterator referring to the past-the-end element in the DequeBase container.
		///
		/// The past-the-end element is the theoretical element that would follow the last element
		/// in the DequeBase container. It does not point to any element, and thus shall not be dereferenced.
		///
		/// Because the ranges used by functions of the standard library do not include the element
		/// pointed by their closing iterator, this function can be used in combination with
		/// DequeBase::begin to specify a range including all the elements in the container.
		///
		/// \return An iterator to the element past the end of the DequeBase
		iterator end();
		/// \brief Returns a const_iterator pointing to the first element in the DequeBase container.
		/// \return A DequeBase to the beginning of the DequeBase container.
		/// \since 1.2.5
		const_iterator begin() const;
		/// \brief Returns a const_iterator referring to the past-the-end element in the DequeBase container.
		/// \return A const_iterator to the element past the end of the DequeBase.
		/// \since 1.2.5
		const_iterator end() const;
		/// \brief Returns a const_iterator pointing to the first element in the DequeBase container.
		/// \return A const_iterator to the beginning of the DequeBase container.
		const_iterator cbegin() const;
		/// \brief Returns a const_iterator referring to the past-the-end element in the DequeBase container.
		/// \return A const_iterator to the element past the end of the DequeBase.
		const_iterator cend() const;
		//@}

		/// \name Constructors & Destructor
		//@{
		/// \brief Create a default empty List with optional name
		DequeBase(const std::string& Name = "[no name]", const std::time_t& modified_time = std::time(nullptr));
		/// \brief Destructor
		~DequeBase();
		/// \brief Fill Constructor
		DequeBase(size_t, const T&, const std::string& Name = "[no name]", const std::time_t& modified_time = std::time(nullptr));
		/// \brief Range Constructor
		DequeBase(const_iterator first, const_iterator last, const std::string& Name = "[no name]", const std::time_t& modified_time = std::time(nullptr));
		/// \brief Copy Constructor
		DequeBase(const DequeBase &);
		/// \brief Assignment Constructor
		DequeBase &operator =(const DequeBase &);
		//@}

		///
		/// \name Element Access
		//@{
		///
		/// \brief Random Write Access to an element in the Deque
		///
		/// \param[in] idx Integer offset into the image with respect to the first element in the sequence (DequeBase::begin())
		/// \return A reference to an element.
		/// \since 1.3
		T& operator[](int idx);
		///
		/// \brief Random Access to an element in the Deque
		///
		/// The fastest and preferred method for reading back elements from a Dequeis to use
		/// const_iterator to retrieve elements in sequence.  In some circumstances however this is
		/// not suitable, and so the array subscript operator is defined to permit applications to
		/// access an ImagePoint at any arbitrary position for readback.
		/// \param[in] idx Integer offset into the image with respect to the first element in the sequence (DequeBase::cbegin())
		/// \return A const reference to an element.
		/// \since 1.0
		const T& operator[](int idx) const;
		//@}

		///
		/// \name DequeBase Unique Identifier
		//@{
		///
		/// \brief Equality Operator checks Deque object UUID's for equivalence
		///
		/// Each Deque object created in software has its own UUID (Universally Unique ID) assigned.
		/// In order to confirm whether two deque objects are identical, their UUIDs are compared.
		/// Deque objects can also be compared with Deques residing on iMS Controller hardware, since
		/// the UUID of a deque is stored in memory on the hardware.
		///
		/// \param[in] rhs A Deque object to perform the comparison with
		/// \return True if the supplied Deque is identical to this one.
		/// \since 1.0.1
		bool operator==(DequeBase const& rhs) const;
		///
		/// \brief Returns a vector representing the Unique Identifier assigned to the DequeBase object
		/// \return UUID as an array of uint8_t's
		const std::array<std::uint8_t, 16> GetUUID() const;
		//@}
		///

		///
		/// \name Timestamping
		//@{
		///
		/// \brief Returns Time at which the Container was last modified
		///
		/// Any time the container is modified (added to, deleted from, elements updated), the system time is recorded.
		/// This happens coincident with the UUID if the container also being updated.  This function returns to the user that timestamp.
		/// \return a reference to a std::time_t representing the time at which the container was last modified
		/// \since 1.3
		const std::time_t& ModifiedTime() const;
		///
		/// \brief Returns Human-readable string for the time at which the Container was last modified
		/// \since 1.3
		std::string ModifiedTimeFormat() const;
		//@}
		///

		///
		/// \name Container Description
		//@{
		///
		/// \brief A string stored with the Container to aid human users in identifying its purpose 
		///
		/// Updating the Container Name does not cause the Container UUID to change.
		const std::string& Name() const;
		std::string& Name();
		//@}
		///

		/// \brief clears the contents
		/// \since 1.3
		void clear();

		/// \brief Inserts a single new element into the DequeBase
		/// \since 1.3
		iterator insert(iterator pos, const T& value);

		/// \brief Inserts multiple copies of an element into the DequeBase
		iterator insert(const_iterator pos, size_t count, const T& value);

		/// \brief Inserts a range of elements into the DequeBase
		iterator insert(iterator pos, const_iterator first, const_iterator last);

		/// \brief Appends the given element value to the end of the container.
		void push_back(const T& value);

		/// \brief Removes the last element of the container
		void pop_back();

		/// \brief Prepends the given element value to the beginning of the container. 
		void push_front(const T& value);

		/// \brief Removes the first element of the container.
		void pop_front();

		/// \brief Removes the element at pos.
		iterator erase(iterator pos);

		/// \brief Removes the elements in the range [first; last]
		iterator erase(iterator first, iterator last);

		/// \brief Returns the number of elements in the container
		std::size_t size() const;

	private:
		class DequeImpl;
		DequeImpl *p_DequeImpl;
	};

}

#endif
