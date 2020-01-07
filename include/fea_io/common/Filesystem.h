/** Duplicates some of the main functionality from C++17's 
 * <experimental/filesystem> which is not available on most of
 * our current Linux compilers. Partially taken from GNU's 8.2.0
 * implementation (gcc-8.2.0/libstdc++-v3/include/experimental) 
 * and from testing results with that compiler and with VS 2017. 
 */

#ifndef UTIL_FILESYSTEM_H
#define UTIL_FILESYSTEM_H

#ifdef HAVE_FILESYSTEM

#   if defined(_WIN32) && !defined(__CYGWIN__)
#       if _HAS_CXX17
#           include <filesystem>
            namespace fs = std::filesystem;
#       else
#           include <experimental/filesystem>
            namespace fs = std::experimental::filesystem;
#       endif
#   else
#           include <experimental/filesystem>
            namespace fs = std::experimental::filesystem;
#   endif

#else

#include <iostream>
#include <stdio.h>    // defines FILENAME_MAX
#include <string> 
#include <sys/stat.h> // provides stat and mkdir

// Platform-independent way to get current dir
#if defined(_WIN32) && !defined(__CYGWIN__)
#    define _GLIBCXX_FILESYSTEM_IS_WINDOWS 1
#    include <direct.h>
#    define GetCurrentDir _getcwd
#else
#    include <unistd.h>
#    define GetCurrentDir getcwd
#endif


// Use same namespace structure as <experimental/filesystem>
namespace std {
namespace experimental {
namespace filesystem {

    class path
    {
    public: 

        // Structors
        path () noexcept { }

        path (const path& __p)
            :
            _pathname(__p._pathname) 
        { }

        path (const std::string& __s)
            :
            _pathname(__s)
        { } 

        ~path() = default; 


        // Construct or update path from a string. Update also works for const char*.
        inline path& operator=(const std::string& __s) noexcept
        {
            _pathname = __s; 
            return *this; 
        }

        // Construct or update path from another path
        inline path& operator=(const path& __p) noexcept
        {
            _pathname = __p._pathname; 
            return *this; 
        }


        // Append string to path, with correct path separators.
        // In both VS and GNU, if original path does not end in a separator,
        // and append path does not start with a separator, then a separator
        // is added. Otherwise, paths are just concatenated.
        inline path& append_path(const std::string& __s)
        {
            if ((_pathname.at(_pathname.size()-1) != separator) &&
                (__s.at(0) != separator))
                _pathname += separator + __s;
            else
                _pathname += __s; 

            return *this; 
        }

        // Append string to path
        inline path& operator/=(const std::string& __s) 
        {
            return(append_path(__s)); 
        }

        // Append another path to path
        inline path& operator/=(const path& __p) 
        {
            return(append_path(__p._pathname)); 
        }

        // Concatenate two paths
        inline path operator/(const path& __rhs) const
        {
            path __result(*this); 
            return (__result /= __rhs); 
        }
    
        // Join a path and a string
        inline path operator/(const std::string& __s)
        {
            path __result(*this); 
            return (__result /= __s); 
        }

        // Compare path to string
        inline bool operator==(const std::string& __s)
        {
            path __result(*this); 
            return(__result.string() == __s); 
        }

        // Compare path to string
        inline bool operator!=(const std::string& __s)
        {
            path __result(*this); 
            return(__result.string() != __s); 
        }

    
        // Return path as a string
        inline std::string string() const {return _pathname;} 

        // Same thing for generic_string
        inline std::string generic_string() const {return _pathname;} 

        // Write a path to a stream. Note in GNU implementation that this 
        // adds quotes around the path.
        friend std::ostream& operator<<(std::ostream& __os, const path& __p)
        {
            return __os << __p._pathname; 
        }

        // Get just the filename (stripped of dir info) for the path. In both VS and GNU,
        // if the path ends in a separator, then the filename returned is ".". Otherwise, 
        // the filename is everything after the last separator. Special cases if path is root: 
        // C:\ returns \ and for Linux / returns /. 
        inline path filename() const
        {
            if (_pathname.size() <= 1) // typically just the separator
                return path(_pathname); 
      
            int pos = _pathname.find_last_of(separator); 

            // If separator at end, check for root, otherwise return "."
            if (pos == _pathname.size()-1) 
            {
                if ((_pathname.size() == 3) && (_pathname.at(1) == ':'))
                    return path(_pathname.substr(2));
                else if (_pathname.size() == 1) 
                    return path(_pathname); 
                else
                    return path("."); 
            }
            else
                return path(_pathname.substr(pos+1,_pathname.size())); 
        }

        // Get the parent path of the path. Generally, if there is a separator at the end, then 
        // the path without this final separator is returned. Two special cases: C:\ returns C:
        // and / returns "". A path without separators returns "". 
        // Thus, for Windows: C:\file -> C:\ -> C: -> ""
        //       for Linux:   /home/file/ -> /home/file -> /home -> / -> ""
        inline path parent_path() const
        {
            std::string parent = _pathname; 

            // Note that both VS and GNU implementations get rid of any 
            // double separators in the parent path
            int pos = 1; 
            std::string seps(2,separator);  
            while (pos != -1)
            {
                pos = parent.find(seps);
                if (pos != -1)
                    parent.erase(pos,1); 
            }

#ifdef _GLIBCXX_FILESYSTEM_IS_WINDOWS 

            // VS implementation additionally changes internal :\ to :
            pos = 1; 
            while (pos != -1)
            {
                if (parent.size() > 3)
                {
                    pos = parent.substr(3).find(":\\");
                    if (pos != -1)
                        parent.erase(pos+4,1); 
                }
                else
                    pos = -1;
            }
        
            pos = parent.find_last_of(separator); 
            if (pos == 2)
            {
                if (parent.size() > 3) 
                    return path(parent.substr(0,3));
                else 
                    return path(parent.substr(0,2)); 
            }
            else if (pos == -1) 
                return path(""); 
            else
                return path(parent.substr(0,pos)); 
#else

            pos = parent.find_last_of(separator); 
            if ((pos == 0) && (parent.size() > 1))
                return path("/"); 
            else if (pos == -1)
                return path("");  
            else
                return path(parent.substr(0,pos)); 
      
#endif
        }


        // Return true if absolute
        inline bool is_absolute() const
        {
#ifdef _GLIBCXX_FILESYSTEM_IS_WINDOWS 
            if ( (_pathname.size() > 1) && (_pathname[1] == ':') )
                return true;
#endif
            if ( (_pathname.size() > 0) && (_pathname[0] == separator) )
                return true;
            else
                return false; 
        }


        // Check if path has extension. Note if you have a separator after a period,
        // this is not counted as an extension.
        inline bool has_extension() const
        {
            int ext_pos = _pathname.find_last_of('.'); 
            int sep_pos = _pathname.find_last_of(separator); 
            if (ext_pos <= sep_pos)
                return false;
            else 
                return true; 
        }


        // Get the extension of the path. If none, return "".
        inline path extension() const
        {
            int ext_pos = _pathname.find_last_of('.'); 
            int sep_pos = _pathname.find_last_of(separator); 
            if (ext_pos <= sep_pos)
                return path("");
            else 
                return path(_pathname.substr(ext_pos,_pathname.size())); 
        }

        // For GCC and MSVC, both replaces and returns
        inline path replace_extension(const std::string& new_ext)
        {
            int ext_pos = _pathname.find_last_of('.'); 
            int sep_pos = _pathname.find_last_of(separator); 
            if (ext_pos <= sep_pos)
                return path(_pathname);
            else {
                _pathname = _pathname.substr(0,ext_pos) + new_ext;  
                return path(_pathname);
            }
            
        }



    private: 
#ifdef _GLIBCXX_FILESYSTEM_IS_WINDOWS 
        static const char separator = '\\';
#else
        static const char separator = '/';
#endif
  
        std::string _pathname; 

    }; // end class path


    // Return absolute path of current working dir (getcwd)
    // source: https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
    static path current_path()
    {
        char cwd[FILENAME_MAX];   // Linux 4096, Win: 260 !!
        if (!GetCurrentDir(cwd, FILENAME_MAX)) {
            std::cout << "current_path(): error!\n";
        }

        std::string cwd_str(cwd);
        return path(cwd_str);
    };


    // Get the absolute value of the path. Note that in both MSVC 
    // and GNU, this returns the given path if already absolute, 
    // including if first character is the path separator. 
    static path absolute(const path &given)
    {
        if (given.is_absolute())
            return path(given.string());
        else
            return current_path() / given;
    };


    // Return true if path exists
    static bool exists(const path& given)
    {
        struct stat buffer;
        return (stat (given.string().c_str(), &buffer) == 0); 
    }

    // Return true if exists and a dir
    static bool is_directory(const path& given)
    {
        struct stat buffer;
        int status = stat (given.string().c_str(), &buffer); 
        return S_ISDIR(buffer.st_mode);  
    }

    // Create a new directory with standard permissions
    static void create_directory(const path& given)
    {
        mkdir(given.string().c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    }




} } } // end namespaces

namespace fs = std::experimental::filesystem;

#endif // End #ifdef HAVE_FILESYSTEM

#endif // UTIL_FILESYSTEM_H
