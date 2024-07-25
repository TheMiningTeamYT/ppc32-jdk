/*
 * Copyright (c) 2016, Oracle Oracle and/or its affiliates. All rights reserved.
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */
// AUTOMATICALLY GENERATED FILE - DO NOT EDIT                                  
package sun.nio.fs;                                                            
import java.security.AccessController;                                         
import java.security.PrivilegedAction;                                         
class UnixConstants {                                                          
    private UnixConstants() { }                                                
    private static int pO_RDONLY=0;
    private static int pO_WRONLY=0;
    private static int pO_RDWR=0;
    private static int pO_APPEND=0;
    private static int pO_CREAT=0;
    private static int pO_EXCL=0;
    private static int pO_TRUNC=0;
    private static int pO_SYNC=0;
    private static int pO_DSYNC=0;
    private static int pO_NOFOLLOW=0;
    private static int pS_IRUSR=0;
    private static int pS_IWUSR=0;
    private static int pS_IXUSR=0;
    private static int pS_IRGRP=0;
    private static int pS_IWGRP=0;
    private static int pS_IXGRP=0;
    private static int pS_IROTH=0;
    private static int pS_IWOTH=0;
    private static int pS_IXOTH=0;
    private static int pS_IFMT=0;
    private static int pS_IFREG=0;
    private static int pS_IFDIR=0;
    private static int pS_IFLNK=0;
    private static int pS_IFCHR=0;
    private static int pS_IFBLK=0;
    private static int pS_IFIFO=0;
    private static int pS_IAMB=0;
    private static int pR_OK=0;
    private static int pW_OK=0;
    private static int pX_OK=0;
    private static int pF_OK=0;
    private static int pENOENT=0;
    private static int pEACCES=0;
    private static int pEEXIST=0;
    private static int pENOTDIR=0;
    private static int pEINVAL=0;
    private static int pEXDEV=0;
    private static int pEISDIR=0;
    private static int pENOTEMPTY=0;
    private static int pENOSPC=0;
    private static int pEAGAIN=0;
    private static int pENOSYS=0;
    private static int pELOOP=0;
    private static int pEROFS=0;
    private static int pENODATA=0;
    private static int pERANGE=0;
    private static int pEMFILE=0;
    private static int pAT_SYMLINK_NOFOLLOW=0;
    private static int pAT_REMOVEDIR=0;
    private static native void init();
    static {
        AccessController.doPrivileged(new PrivilegedAction<Void>() {
            public Void run() {
                System.loadLibrary("nio");
                return null;
        }});
        init();
    }
    static final int O_RDONLY = pO_RDONLY;
    static final int O_WRONLY = pO_WRONLY;
    static final int O_RDWR = pO_RDWR;
    static final int O_APPEND = pO_APPEND;
    static final int O_CREAT = pO_CREAT;
    static final int O_EXCL = pO_EXCL;
    static final int O_TRUNC = pO_TRUNC;
    static final int O_SYNC = pO_SYNC;
    static final int O_DSYNC = pO_DSYNC;
    static final int O_NOFOLLOW = pO_NOFOLLOW;
    static final int S_IRUSR = pS_IRUSR;
    static final int S_IWUSR = pS_IWUSR;
    static final int S_IXUSR = pS_IXUSR;
    static final int S_IRGRP = pS_IRGRP;
    static final int S_IWGRP = pS_IWGRP;
    static final int S_IXGRP = pS_IXGRP;
    static final int S_IROTH = pS_IROTH;
    static final int S_IWOTH = pS_IWOTH;
    static final int S_IXOTH = pS_IXOTH;
    static final int S_IFMT = pS_IFMT;
    static final int S_IFREG = pS_IFREG;
    static final int S_IFDIR = pS_IFDIR;
    static final int S_IFLNK = pS_IFLNK;
    static final int S_IFCHR = pS_IFCHR;
    static final int S_IFBLK = pS_IFBLK;
    static final int S_IFIFO = pS_IFIFO;
    static final int S_IAMB = pS_IAMB;
    static final int R_OK = pR_OK;
    static final int W_OK = pW_OK;
    static final int X_OK = pX_OK;
    static final int F_OK = pF_OK;
    static final int ENOENT = pENOENT;
    static final int EACCES = pEACCES;
    static final int EEXIST = pEEXIST;
    static final int ENOTDIR = pENOTDIR;
    static final int EINVAL = pEINVAL;
    static final int EXDEV = pEXDEV;
    static final int EISDIR = pEISDIR;
    static final int ENOTEMPTY = pENOTEMPTY;
    static final int ENOSPC = pENOSPC;
    static final int EAGAIN = pEAGAIN;
    static final int ENOSYS = pENOSYS;
    static final int ELOOP = pELOOP;
    static final int EROFS = pEROFS;
    static final int ENODATA = pENODATA;
    static final int ERANGE = pERANGE;
    static final int EMFILE = pEMFILE;
    static final int AT_SYMLINK_NOFOLLOW = pAT_SYMLINK_NOFOLLOW;
    static final int AT_REMOVEDIR = pAT_REMOVEDIR;
}                                                                              
