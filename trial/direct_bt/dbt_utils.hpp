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


class DBTUtils {
    private:
        static int get_stat_mode(const std::string& name, bool& no_access, bool& not_existing, bool& is_file, bool& is_dir) {
            no_access = false;
            not_existing = false;
            is_file = false;
            is_dir = false;

            struct stat s;
            if( 0 != stat(name.c_str(), &s) ) {
                switch( errno ) {
                    case EACCES:
                        no_access = true;
                        break;
                    case ENOENT:
                        not_existing = true;
                        break;
                    default:
                        break;
                }
                jau::fprintf_td(stderr, "****** PATH '%s': stat errno: %d %s\n",
                        name.c_str(), errno, strerror(errno));
                return errno;
            }
            is_file = S_ISDIR( s.st_mode );
            is_dir = S_ISREG( s.st_mode );
            return 0;
        }

    public:
        static bool is_existing(const std::string& name) {
            bool no_access, not_existing, is_file, is_dir;
            const bool res0 = 0 == get_stat_mode(name, no_access, not_existing, is_file, is_dir);
            return res0 && !not_existing;
        }

        static bool is_dir(const std::string& name) {
            bool no_access, not_existing, is_file, is_dir;
            const bool res0 = 0 == get_stat_mode(name, no_access, not_existing, is_file, is_dir);
            return res0 && is_dir;
        }

        static bool is_file(const std::string& name) {
            bool no_access, not_existing, is_file, is_dir;
            const bool res0 = 0 == get_stat_mode(name, no_access, not_existing, is_file, is_dir);
            return res0 && is_file;
        }

        static bool mkdir(const std::string& name) {
            bool no_access, not_existing, is_file, is_dir;
            const int errno_res = get_stat_mode(name, no_access, not_existing, is_file, is_dir);
            if( 0 != errno_res && !not_existing) {
                jau::fprintf_td(stderr, "****** PATH '%s': stat failed: %d %s\n", name.c_str(), errno_res, strerror(errno_res));
                return false;
            }
            if( !not_existing && is_dir ) {
                jau::fprintf_td(stderr, "****** PATH '%s': dir already exists\n", name.c_str());
                return true;
            } else if( not_existing ) {
                const int dir_err = ::mkdir(name.c_str(), S_IRWXU | S_IRWXG ); // excluding others
                if ( 0 != dir_err ) {
                    jau::fprintf_td(stderr, "****** PATH '%s': mkdir failed: %d %s\n", name.c_str(), errno, strerror(errno));
                    return false;
                } else {
                    return true;
                }
            } else {
                jau::fprintf_td(stderr, "****** PATH '%s': entity exists but is no dir\n", name.c_str());
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
                  if( 0 == fname.find("bd_") ) { // prefix checl
                      const jau::nsize_t suffix_pos = fname.size() - 4;
                      if( suffix_pos == fname.find(".key", suffix_pos) ) { // suffix check
                          res.push_back( dname + "/" + fname ); // full path
                      }
                  }
              }
              ::closedir (dir);
            } // else: could not open directory
            return res;
        }

        static bool mkdirKeyFolder() {
            if( mkdir( DBTConstants::CLIENT_KEY_PATH ) ) {
                if( mkdir( DBTConstants::SERVER_KEY_PATH ) ) {
                    return true;
                }
            }
            return false;
        }

        /**
         *
         * @param file
         * @param recursive
         * @return true only if the file or the directory with content has been deleted, otherwise false
         */
        static bool remove(const std::string& file, const bool recursive) {
            (void) recursive;
            if( 0 != ::remove( file.c_str() ) ) {
                if( ENOENT == errno ) {
                    // not existing
                    return true;
                } else {
                    jau::fprintf_td(stderr, "****** PATH '%s': remove failed: %d %s\n", file.c_str(), errno, strerror(errno));
                    return false;
                }
            } else {
                return true;
            }
#if 0
            bool rm_parent = true;
            std::vector<std::string> contents = get_file_list(file);
            if ( contents.size() > 0 ) {
                for (const std::string& f : contents) {
                    bool no_access, not_existing, is_file, is_dir;
                    const int errno_res = get_stat_mode(f, no_access, not_existing, is_file, is_dir);
                    if( 0 != errno_res && !not_existing) {
                        jau::fprintf_td(stderr, "****** PATH '%s': stat failed: %d %s\n", f.c_str(), errno_res, strerror(errno_res));
                        return false;
                    }
                    if( not_existing ) {
                        jau::fprintf_td(stderr, "****** PATH '%s': listed entity not existing: %d %s\n", f.c_str(), errno_res, strerror(errno_res));
                        // try to continue ..
                    } else if( is_dir ) {
                        if( recursive ) {
                            rm_parent = remove(f, true) && rm_parent;
                        } else {
                            // can't empty contents -> can't rm 'file'
                            rm_parent = false;
                        }
                    } else /* assume is_file or link .. */ {
                        rm_parent = f.delete() && rm_parent;
                    }
                }
            }
            if( rm_parent ) {
                try {
                    return file.delete();
                } catch( final Exception e ) { e.printStackTrace(); }
            }
            return false;
#endif
        }

        static bool rmKeyFolder() {
            if( remove( DBTConstants::CLIENT_KEY_PATH, true ) ) {
                if( remove( DBTConstants::SERVER_KEY_PATH, true ) ) {
                    return true;
                }
            }
            return false;
        }
};

#endif /* DBT_UTILS_HPP_ */
