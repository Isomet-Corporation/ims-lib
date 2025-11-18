/*-----------------------------------------------------------------------------
/ Title      : Version Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/LibVersion.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-08 22:15:19 +0000 (Wed, 08 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 658 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file LibVersion.h
/// 
/// \brief Access the API's version information
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Versioning
///


#ifndef IMS_LIBVERSION_H__
#define IMS_LIBVERSION_H__

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

///
//@{
///
/// \brief Major Version Number
///
/// The API Major Version number for use in preprocessing directives
///
#define IMS_API_MAJOR 2
///
/// \brief Minor Version Number
///
/// The API Minor Version number for use in preprocessing directives
///
#define IMS_API_MINOR 0
///
/// \brief Patch Version Number
///
/// The API Patch Version number for use in preprocessing directives
///
#define IMS_API_PATCH 3
//@}

/// \namespace iMS
/// \brief The entire API is encapsulated by the iMS namespace
///
/// In User application code, either add the line 'using namespace iMS;' at the start of your application, or prefix all classes, functions etc with 'iMS::'
/// \author Dave Cowan
/// \since 1.0
namespace iMS
{
    /// 
    /// \class LibVersion LibVersion.h include/LibVersion.h
    /// \brief Access the version information for the API
    /// 
    /// For example, you can get the current version number as
    /// a string using \c GetVersion, or you can get the separate
    /// major, minor and patch integer values by calling
    /// \c GetMajor, \c GetMinor, or \c GetPatch, respectively.
    ///
    /// This class also provides some basic version comparison
    /// functionality and lets you determine if certained named
    /// features are present in your current build.
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC LibVersion
	{
	public:
        /// \name Version Numbers
        //@{
        ///
        /// \brief Return the major version number, e.g., 1 for "1.2.3"
        /// \return The major version number as an integer
        /// \since 1.0
        ///
		static int GetMajor();
		///
		/// \brief Return the minor version number, e.g., 2 for "1.2.3"
		/// \return The minor version number as an integer
		/// \since 1.0
		///
		static int GetMinor();
		///
		/// \brief Return the patch version number, e.g., 3 for "1.2.3"
		/// \return The patch version number as an integer
		/// \since 1.0
		///
		static int GetPatch();

        ///
		/// \brief Return the full version number 
		/// \return The version string, e.g., "1.2.3"
		/// \since 1.0
		///
		static std::string GetVersion();
		//@}

        /// \name Version Number Maths
        //@{
        ///
        /// \brief Compare the current version number against a specific 
        /// version.
        ///
        /// This method lets you check to see if the current version 
        /// is greater than or equal to the specified version.  This may
        /// be useful to perform operations that require a minimum
        /// version number.
        ///
        /// \param[in] major The major version number to compare against
        /// \param[in] minor The minor version number to compare against
        /// \param[in] patch The patch version number to compare against
        /// \return Returns true if the current API version >= (major, minor, patch)
        /// \since 1.0
        ///
		static bool IsAtLeast(int major, int minor, int patch);
		//@}

        /// \name Feature Tags
        //@{
        ///
        /// \brief Test whether a feature is implemented by this API.
        ///
        /// New features that change the implementation of API methods 
        /// are specified as "feature tags." This method lets you
        /// query the API to find out if a given feature is available.
        ///
        /// \param[in] name The feature tag name, e.g., "IMAGE_FILE"
        /// \return Returns true if the named feature is available in this version
        /// \since 1.0
        ///
		static bool HasFeature(const std::string &name);
		//@}
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
