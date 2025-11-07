/*-----------------------------------------------------------------------------
/ Title      : Isomet Image Private Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ImageOps/h/Image_p.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-08 21:34:12 +0000 (Wed, 08 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 655 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file Image_p.h
///
/// \brief Classes for storing sequences of synchronous multi-channel RF drive data
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Image
///

#ifndef IMS_IMAGE_P_H__
#define IMS_IMAGE_P_H__

//#include "IMSTypeDefs.h"
#include "Image.h"
#include <list>

namespace iMS {
	class ImageTable
	{
	public:
		ImageTable();

		typedef std::list<ImageTableEntry>::iterator iterator;
		typedef std::list<ImageTableEntry>::const_iterator const_iterator;

		iterator begin() {return m_tbl.begin();}
		iterator end() {return m_tbl.end();}
		const_iterator begin() const { return m_tbl.cbegin(); }
		const_iterator end() const { return m_tbl.cend(); }
		const_iterator cbegin() const { return begin(); }
		const_iterator cend() const { return end(); }

		bool empty() const { return m_tbl.empty(); }
		std::size_t size() const { return m_tbl.size(); }

		void assign(size_t n, const ImageTableEntry& val) { m_tbl.assign(n, val); }
		void push_front(const ImageTableEntry& val) { m_tbl.push_front(val); }
		void pop_front() { m_tbl.pop_front(); }
		void push_back(const ImageTableEntry& val) { m_tbl.push_back(val); }
		void pop_back() { m_tbl.pop_back(); }
		iterator insert(iterator position, const ImageTableEntry& val) { return m_tbl.insert(position, val); }
		iterator erase(iterator position) { return m_tbl.erase(position);}
		void resize(size_t n) { m_tbl.resize(n, ImageTableEntry()); }
		void clear() { m_tbl.clear(); }
	private:
		std::list < ImageTableEntry > m_tbl;
	};

	class ImageTableReader
	{
	public:
		ImageTableReader(std::shared_ptr<IMSSystem> ims);
		~ImageTableReader();
		ImageTable Readback();
	private:
		// Make this object non-copyable
		ImageTableReader(const ImageTableReader &);
		const ImageTableReader &operator =(const ImageTableReader &);

		// Declare IMS System
		std::weak_ptr<IMSSystem> m_ims;
	};

}

#endif