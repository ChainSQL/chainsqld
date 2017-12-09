//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#include <BeastConfig.h>
#include <ripple/core/Config.h>
#include <peersafe/basics/characterUtilities.h>
#ifndef _WIN32
#include <iconv.h>
#endif

namespace ripple {
    
#ifdef _WIN32
    std::string TransferBetweenGBKandUTF8(const std::string& strGBK, bool bFromGBK2UTF8)
    {
        auto uSrc = CP_ACP, uDest = CP_UTF8;
        if (!bFromGBK2UTF8)
        {
            uSrc = CP_UTF8; uDest = CP_ACP;
        }
        std::string strOutUTF8 = "";
        WCHAR * str1;
        int n = MultiByteToWideChar(uSrc, 0, strGBK.c_str(), -1, NULL, 0);
        str1 = new WCHAR[n];
        MultiByteToWideChar(uSrc, 0, strGBK.c_str(), -1, str1, n); n = WideCharToMultiByte(uDest, 0, str1, -1, NULL, 0, NULL, NULL);
        char * str2 = new char[n];
        WideCharToMultiByte(uDest, 0, str1, -1, str2, n, NULL, NULL);
        strOutUTF8 = str2;
        delete[]str1;
        str1 = NULL;
        delete[]str2;
        str2 = NULL;
        return strOutUTF8;
    }
#else
    int code_convert(const char *from_charset, const char *to_charset, char *inbuf, size_t inlen, char *outbuf, size_t outlen)
    {
        iconv_t cd;
        char **pin = &inbuf;
        char **pout = &outbuf;

        cd = iconv_open(to_charset, from_charset);
        if (cd == 0)
            return -1;
        memset(outbuf, 0, outlen);
        if (iconv(cd, pin, &inlen, pout, &outlen) == -1)
            return -1;
        iconv_close(cd);
        return 0;
    }

    int u2g(char *inbuf, int inlen, char *outbuf, size_t outlen)
    {
        return code_convert("utf-8", "gb2312", inbuf, inlen, outbuf, outlen);
    }

    int g2u(char *inbuf, size_t inlen, char *outbuf, size_t outlen)
    {
        return code_convert("gb2312", "utf-8", inbuf, inlen, outbuf, outlen);
    }
#endif

    bool  TransGBK_UTF8(const std::string &sSrc, std::string &sDest, bool bFromGBK2UTF8)
    {                
        sDest.clear();
#ifdef _WIN32
        sDest = TransferBetweenGBKandUTF8(sSrc, bFromGBK2UTF8);
#else
        sDest = sSrc;
        return true;

        char outbuf[1024] = { 0 };
        char inbuf[1024] = { 0 };
        std::string fromstr = sSrc;
        fromstr.copy(inbuf, fromstr.length(), 0);
        
        bool bRet = false;
        if (bFromGBK2UTF8)
        {
            bRet = g2u(inbuf, fromstr.length(), outbuf, 1024) == 0;
        }
        else
        {
            bRet= u2g(inbuf, fromstr.length(), outbuf, 1024) == 0;
        }
        if (bRet)     sDest = outbuf;
#endif       

        if (sDest.empty())                return false;

        return  true;
    }
}
