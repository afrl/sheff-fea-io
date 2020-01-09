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
#include <fstream>
#include <stdio.h>    // defines FILENAME_MAX
#include <string> 
#include <string.h>   // provides strcmp
#include <sys/stat.h> // provides stat and mkdir
#include <dirent.h>   // provides dirent
#include <system_error>

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


#ifdef _GLIBCXX_FILESYSTEM_IS_WINDOWS 
    static const char separator = '\\';
#else
    static const char separator = '/';
#endif

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
        inline path& append(const std::string& __s)
        {
            if ((_pathname.at(_pathname.size()-1) != separator) &&
                (__s.at(0) != separator))
                _pathname += separator + __s;
            else
                _pathname += __s; 

            return *this; 
        }

        // Evidently GCC filesystem doesn't define this
        //inline path& append(const path& __p)
        //{
        //    return(append(__p._pathname)); 
        //}


        // Append string to path
        inline path& operator/=(const std::string& __s) 
        {
            return(append(__s)); 
        }

        // Append another path to path
        inline path& operator/=(const path& __p) 
        {
            return(append(__p._pathname)); 
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

        // Compare path to path
        inline bool operator==(const path& __p)
        {
            path __result(*this); 
            return(__result.string() == __p.string()); 
        }

        // Compare path to string
        inline bool operator!=(const std::string& __s)
        {
            path __result(*this); 
            return(__result.string() != __s); 
        }

        // Compare path to path
        inline bool operator!=(const path& __p)
        {
            path __result(*this); 
            return(__result.string() != __p.string()); 
        }


        // Concat path with string (no separator added) 
        inline void operator+=(const std::string& __s)
        {
            _pathname += __s; 
        }

        // Concat path with path (no separator added) 
        inline void operator+=(const path& __p)
        {
            _pathname += __p._pathname; 
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
        // this is not counted as an extension. Also, at least in GCC, the endings /.. and /.
        // are not counted as extensions. 
        inline bool has_extension() const
        {
            int ext_pos = _pathname.find_last_of('.'); 
            int sep_pos = _pathname.find_last_of(separator); 
            std::string end1 = {separator, '.' , '.'}; 
            std::string end2 = {separator, '.'}; 
            
            if ( (ext_pos == -1) || (ext_pos < sep_pos) )               //  a.b/c 
                return false;
            if ( (_pathname.size() > 2) && 
                 (_pathname.substr(ext_pos-2,3) == end1) )  //  a/..
                return false;
            if ( (_pathname.size() > 1) && 
                 (_pathname.substr(ext_pos-1,2) == end2) )   //  a/.
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


        // For GCC and MSVC, both replaces and returns. 
        // If has an extension, replaces, otherwise, adds new extension
        inline path replace_extension(const std::string& new_ext)
        {
            if (has_extension()) {
                int ext_pos = _pathname.find_last_of('.'); 
                _pathname = _pathname.substr(0,ext_pos) + new_ext;  
                return path(_pathname);
            }
            else {
                _pathname += new_ext; 
                return path(_pathname); 
            } 
            
        }

        // Get the stem of the path (filename without extension) 
        inline path stem()
        {
            path ret = path(_pathname).filename(); 
            if (ret.has_extension()) 
                ret.replace_extension(""); 
            return(ret); 
        }


    private: 
        std::string _pathname; 

    }; // end class path


    // Class for filesystem errors. I only implement a single constructor and 
    // prefix what() message. Override what() method since otherwise ec.code().message() is added.
    //see https://en.cppreference.com/w/cpp/error/errno_macros for list of error codes
    class filesystem_error : public std::system_error
    {
        public:
            filesystem_error(const std::string& __what_arg, int __ec)
              : 
              std::system_error(__ec, std::generic_category(), "filesystem hack error: " + __what_arg),
              err_msg("filesystem hack error: " + __what_arg)
            { }; 

            ~filesystem_error(){};

            const char* what() const noexcept { return err_msg.c_str(); };  

        private:
            std::string err_msg; 
    }; 


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
    static path absolute(const path& given)
    {
        if (given.is_absolute())
            return path(given.string());
        else
            return current_path() / given;
    };

    static path absolute(const std::string& given)
    {
        path p(given); 
        if (p.is_absolute())
            return p;
        else
            return current_path() / p;
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

    // Return absolute path with dot or dot-dot elements converted to real path.
    // Give error if path does not exist
    static path canonical(const path& given)
    {
        if (!exists(given)) {
            std::string err_msg = "cannot canonicalize: No such file or directory [" + 
                given.string() + "]"; 

            throw filesystem_error(err_msg, ENOENT);  
        }

        std::string canon = absolute(given).string(); 

        // Ok, first collapse /./ to /
        std::string sdot = {separator, '.', separator}; 
        int dot_pos = canon.find(sdot);
        while (dot_pos != -1)
        {
            canon.erase(dot_pos,2); 
            dot_pos = canon.find(sdot); 
        }

        // Then Collapse  a/b/../c to a/c 
        std::string ddot = {separator, '.' , '.', separator}; 
        dot_pos = canon.find(ddot);    // Need to collapse left to right
        int sep_pos; 
        while (dot_pos != -1)
        {
            sep_pos = canon.substr(0,dot_pos).find_last_of(separator); 
            if (sep_pos != -1)  
                canon.erase(sep_pos, dot_pos - sep_pos + 3); 

            dot_pos = canon.find(ddot);
        }

        return path(canon); 
    };

    static path canonical (const std::string& s)
    {
        return canonical(path(s)); 
    }


    // Create a new directory with standard permissions
    static void create_directory(const path& given)
    {
        mkdir(given.string().c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
    }

    // Return true if file or directory is empty
    static bool is_empty( const path& given) 
    {
        if (!exists(given)) {
            std::string err_msg = "cannot check if file is empty: No such file or directory [" + given.string() + "]"; 
            throw filesystem_error(err_msg, ENOENT);  
        }

        if ( is_directory(given) ) {
            struct dirent *entry; 
            DIR *dir = opendir(given.string().c_str());
            // Above should have already caught this...
            if (dir == NULL) {
                std::string err_msg = "not a dir or doesn't exist[" + given.string() + "]"; 
                throw filesystem_error(err_msg, ENOENT);  
            }

            int n = 0; 
            while ((entry = readdir(dir)) != NULL) {
                if (++n > 2)            // First two are always . and ..
                    break; 
            }
            closedir(dir); 
            return (n == 2); 
        }
        else {
            std::ifstream test(given.string(), std::ifstream::binary); 
            test.seekg(0, test.end); 
            int len = test.tellg(); 
            test.close(); 
            return(len == 0);  
        }

        return false; 
    }

    // Remove a file
    static bool remove(const path& given)
    {
        if ( is_directory(given) && !is_empty(given) )   
        {
            std::string err_msg = "cannot remove: Directory not empty [" + given.string() + "]"; 
            throw filesystem_error(err_msg, ENOTEMPTY);  
        }
        try{
            if (0)  // DEBUGGING 
                cout << "removing: " << given.string() << endl; 
            else
                std::remove(given.string().c_str()); 
            return true; 
        } catch (filesystem_error& ec) {
           std::cout << ec.what() << endl;  
        }
        return false; 
    }
    static bool remove(const std::string& given)
    {
        return (remove(path(given))); 
    }

    // Remove a folder and all of its contents
    // GCC version returns the number of deleted files/dirs, but I don't...
    static bool remove_all(const path& given) 
    {
        // Check if dir and remove if not
        if ( !is_directory(given) ) 
        {
            return (remove(given)); 
        }
        else if (is_empty(given)) 
        {
            return (remove(given)); 
        }
            

        // Otherwise, get the children
        struct dirent *entry; 
        DIR *dir = opendir(given.string().c_str());
        // Above should have already caught this...
        if (dir == NULL) {
            std::string err_msg = "not a dir or doesn't exist[" + given.string() + "]"; 
            throw filesystem_error(err_msg, ENOENT);  
        }

        int n = 0; 
        while ((entry = readdir(dir)) != NULL) {
            if (++n < 3)                    // First two are always . and ..
                continue;
            path sub(given); 
            sub /= std::string(entry->d_name); 
            remove_all(sub);  
        }
        closedir (dir); 

        return (remove(given)); 
    }

    static bool remove_all(const std::string& given)
    {
        return (remove_all(path(given))); 
    } 



    /// Bitmask type
    // Since only implementing copy_file, I only implement the copy_file options.
    // I also don't want to deal with checking timestamps, so I don't implement update_existing.
    enum class copy_options : unsigned short {
      none = 0,
      skip_existing = 1, overwrite_existing = 2, //update_existing = 4,
    };

    // Copy a file
    static bool copy_file(const path& a, const path& b, copy_options opts = copy_options::none)
    {
        if (!exists(a)) {
            std::string err_msg = "cannot copy file: No such file or directory [" + 
                a.string() + "]"; 

            throw filesystem_error(err_msg, ENOENT);  
        }

        if (exists(b)) {

            if (opts == copy_options::skip_existing)
                return false; 
            else if (opts != copy_options::overwrite_existing) {

                std::string err_msg = "cannot copy file: File exists [" + 
                    b.string() + "]"; 

                throw filesystem_error(err_msg, EEXIST);  
            }
        }

                    
        try {
            std::ifstream from(a.string(), std::ios::binary); 
            std::ofstream to(b.string(), std::ios::binary); 
            to << from.rdbuf(); 

            return true; 
        } catch (filesystem_error& ec) {
           std::cout << ec.what() << endl;  
        }

        return false; 
    }
                 



} } } // end namespaces

namespace fs = std::experimental::filesystem;

#endif // End #ifdef HAVE_FILESYSTEM

#endif // UTIL_FILESYSTEM_H
