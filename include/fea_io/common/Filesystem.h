#pragma once
/** Duplicates some of the main functionality from C++17's 
 * <experimental/filesystem> which is not available on most of
 * our current Linux compilers. Partially taken from GNU's 8.2.0
 * implementation and from testing results with that compiler
 * and with VS 2017. 
 */

#include <iostream>
#include <stdio.h> // defines FILENAME_MAX
#include <string> 

// TODO: If on Windows, could also check if have high
// enough version to include experimental/filesystem directly.
#if defined(_WIN32) && !defined(__CYGWIN__)
  #define _GLIBCXX_FILESYSTEM_IS_WINDOWS 1
  #include <direct.h>
  #define GetCurrentDir _getcwd
#else
  #include <unistd.h>
  #define GetCurrentDir getcwd
#endif


// Use same namespace structure as <experimental/filesystem>
namespace std { namespace experimental { namespace filesystem {

  class path
  {
    public: 

      // Structors
      path () noexcept { }

      path (const path& __p)
        :
        _pathname(__p._pathname) 
      { }

      path (const std::string __s)
        :
        _pathname(__s)
      { } 

      ~path() = default; 

      // Update path from string 
      inline path& operator=(const std::string __s) noexcept
      {
        _pathname = __s; 
        return *this; 
      }

      // Copy path from another path
      inline path& operator=(const path& __p) noexcept
      {
        _pathname = __p._pathname; 
        return *this; 
      }

      // Append string to path, with correct path separators.
      // In both VS and GNU, if original path does not end in a separator and append path does not start 
      // with a separator, then a separator is added. Otherwise, paths are just concatenated.
      path& append_path(const std::string& __s)
      {
        if ((_pathname.at(_pathname.size()-1) != separator) &&
            (__s.at(0) != separator))
          _pathname += separator + __s;
        else
          _pathname += __s; 

        return *this; 
      }

      // Append string to path
      inline path& operator/=(const std::string __s) 
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
    
      // Return path as a string
      std::string string() const {return _pathname;} 

      // Write a path to a stream. Note in GNU implementation that this 
      // adds quotes around the path.
      friend std::ostream& operator<<(std::ostream& __os, const path& __p)
      {
        return __os << __p._pathname; 
      }

      // Get just the filename (stripped of dir info) for the path. In both VS and GNU,
      // if the path ends in a separator, then the filename returned is ".". Otherwise, 
      // the filename is everything after the last separator. Special case if path is root, 
      // so C:\ returns \ and for Linux / returns /. 
      path filename() const
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
      path parent_path() const
      {
        std::string parent = _pathname; 

        // Note that both VS and GNU implementations get rid of any double separators in the parent path
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


      // Check if path has extension. Note if you have a separator after a period, this is not counted as an extension.
      bool has_extension() const
      {
        int ext_pos = _pathname.find_last_of('.'); 
        int sep_pos = _pathname.find_last_of(separator); 
        if (ext_pos <= sep_pos)
          return 0;
        else 
          return 1; 
      }


      // Get the extension of the path. If none, return "".
      path extension() const
      {
        int ext_pos = _pathname.find_last_of('.'); 
        int sep_pos = _pathname.find_last_of(separator); 
        if (ext_pos <= sep_pos)
          return path("");
        else 
          return path(_pathname.substr(ext_pos,_pathname.size())); 
      }


  private: 
    static const char separator;
  
    std::string _pathname; 

    };


#ifdef _GLIBCXX_FILESYSTEM_IS_WINDOWS 
  const char path::separator = '\\';
#else
  const char path::separator = '/';
#endif

// Return absolute path of current working dir (getcwd)
// source: https://stackoverflow.com/questions/143174/how-do-i-get-the-directory-that-a-program-is-running-from
path current_path()
{
  char cwd[FILENAME_MAX];   // Linux 4096, Win: 260 !!
  if (!GetCurrentDir(cwd, FILENAME_MAX))
    std::cout << "current_path(): error!" << std::endl;  

  std::string cwd_str(cwd); 
  return path(cwd_str); 
};

} } } // end namespaces
