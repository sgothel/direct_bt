/**
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2022 Gothel Software e.K.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef DBT_UTILS_HPP_
#define DBT_UTILS_HPP_

#include <cstring>
#include <string>
#include <memory>
#include <cstdint>
#include <cstdio>

#include "dbt_constants.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

class file_stats {
    private:
        std::string fname_;
        bool access_, exists_, is_link_, is_file_, is_dir_;
        int errno_res_;

    public:
        file_stats(const std::string& fname, const bool use_lstat=true) noexcept
        : fname_(fname), access_(true), exists_(true),
          is_link_(false), is_file_(false), is_dir_(false),
          errno_res_(0)
        {
            struct stat s;
            int stat_res;
            if( use_lstat ) {
                stat_res = ::lstat(fname.c_str(), &s);
            } else {
                stat_res = ::stat(fname.c_str(), &s);
            }
            if( 0 != stat_res ) {
                switch( errno ) {
                    case EACCES:
                        access_ = false;
                        break;
                    case ENOENT:
                        exists_ = false;
                        break;
                    default:
                        break;
                }
                if( access_ && exists_ ) {
                    errno_res_ = errno;
                }
            } else {
                is_link_ = S_ISLNK( s.st_mode );
                is_file_ = S_ISREG( s.st_mode );
                is_dir_ = S_ISDIR( s.st_mode );
            }
        }

        int errno_res() const noexcept { return errno_res_; }
        bool ok()  const noexcept { return 0 == errno_res_; }

        bool has_access() const noexcept { return access_; }
        bool exists() const noexcept { return exists_; }

        bool is_link() const noexcept { return is_link_; }
        bool is_file() const noexcept { return is_file_; }
        bool is_dir() const noexcept { return is_dir_; }

        std::string to_string() const noexcept {
            const std::string e = 0 != errno_res_ ? ", "+std::string(::strerror(errno_res_)) : "";
            return "stat['"+fname_+"', access "+std::to_string(access_)+", exists "+std::to_string(exists_)+
                    ", link "+std::to_string(is_link_)+", file "+std::to_string(is_file_)+
                    ", dir "+std::to_string(is_dir_)+", errno "+std::to_string(errno_res_)+e+"]";
        }
};

class file_utils {
    public:
        static bool mkdir(const std::string& name) {
            file_stats fstats(name);

            if( !fstats.ok() ) {
                jau::fprintf_td(stderr, "mkdir stat failed: %s\n", fstats.to_string().c_str());
                return false;
            }
            if( fstats.is_dir() ) {
                jau::fprintf_td(stderr, "mkdir failed: %s, dir already exists\n", fstats.to_string().c_str());
                return true;
            } else if( !fstats.exists() ) {
                const int dir_err = ::mkdir(name.c_str(), S_IRWXU | S_IRWXG ); // excluding others
                if ( 0 != dir_err ) {
                    jau::fprintf_td(stderr, "mkdir failed: %s, errno %d (%s)\n", fstats.to_string().c_str(), errno, strerror(errno));
                    return false;
                } else {
                    return true;
                }
            } else {
                jau::fprintf_td(stderr, "mkdir failed: %s, exists but is no dir\n", fstats.to_string().c_str());
                return false;
            }
        }

        static std::vector<std::string> get_file_list(const std::string& dname) {
            std::vector<std::string> res;
            DIR *dir;
            struct dirent *ent;
            if( ( dir = ::opendir( dname.c_str() ) ) != nullptr ) {
              while ( ( ent = ::readdir( dir ) ) != NULL ) {
                  std::string fname( ent->d_name );
                  if( "." != fname && ".." != fname ) { // avoid '.' and '..'
                      res.push_back( dname + "/" + fname ); // full path
                  }
              }
              ::closedir (dir);
            } else {
                // could not open directory
                file_stats fstats(dname);
                jau::fprintf_td(stderr, "get_file_list failed: %s, errno %d (%s)\n",
                        fstats.to_string().c_str(), errno, strerror(errno));
            }
            return res;
        }

        /**
         *
         * @param file
         * @param recursive
         * @return true only if the file or the directory with content has been deleted, otherwise false
         */
        static bool remove(const std::string& file, const bool recursive) {
            file_stats fstats_parent(file);
            bool rm_parent = true;
            if( fstats_parent.is_dir() ) {
                std::vector<std::string> contents = get_file_list(file);
                if ( contents.size() > 0 ) {
                    for (const std::string& f : contents) {
                        file_stats fstats(f);
                        if( !fstats.ok() ) {
                            jau::fprintf_td(stderr, "remove: stat failed: %s\n", fstats.to_string().c_str());
                            return false;
                        }
                        if( !fstats.exists() ) {
                            jau::fprintf_td(stderr, "remove: listed entity not existing: %s\n", fstats.to_string().c_str());
                            // try to continue ..
                        } else if( fstats.is_link() ) {
                            jau::fprintf_td(stderr, "remove: listed entity is link (drop): %s\n", fstats.to_string().c_str());
                        } else if( fstats.is_dir() ) {
                            if( recursive ) {
                                rm_parent = remove(f, true) && rm_parent;
                            } else {
                                // can't empty contents -> can't rm 'file'
                                rm_parent = false;
                            }
                        } else if( fstats.is_file() ) {
                            const int res = ::remove( f.c_str() );
                            rm_parent = 0 == res && rm_parent;
                            if( 0 != res ) {
                                jau::fprintf_td(stderr, "remove.1 failed: %s, res %d, errno %d (%s)\n",
                                        fstats.to_string().c_str(), res, errno, strerror(errno));
                            }
                        }
                    }
                }
            }
            if( rm_parent ) {
                const int res = ::remove( file.c_str() );
                if( 0 != res ) {
                    jau::fprintf_td(stderr, "remove.2 failed: %s, res %d, errno %d (%s)\n",
                            fstats_parent.to_string().c_str(), res, errno, strerror(errno));
                }
                return 0 == res;
            }
            return false;
        }
};

class DBTUtils {
    public:

        static bool mkdirKeyFolder() {
            if( file_utils::mkdir( DBTConstants::CLIENT_KEY_PATH ) ) {
                if( file_utils::mkdir( DBTConstants::SERVER_KEY_PATH ) ) {
                    return true;
                }
            }
            return false;
        }


        static bool rmKeyFolder() {
            if( file_utils::remove( DBTConstants::CLIENT_KEY_PATH, true ) ) {
                if( file_utils::remove( DBTConstants::SERVER_KEY_PATH, true ) ) {
                    return true;
                }
            }
            return false;
        }
};

#endif /* DBT_UTILS_HPP_ */
