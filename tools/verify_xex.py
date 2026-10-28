#!/usr/bin/env python3
"""Independently check MK360's system-DLL structure and SHA-1 chains.

These are file checks, not an Xbox loader or hardware compatibility test.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

PAGE=0x10000
BASE=0x81F00000
def require(condition,message):
    if not condition:
        raise ValueError(message)
def u32(data,offset):
    require(0<=offset<=len(data)-4,"32-bit field outside file")
    return struct.unpack_from(">I",data,offset)[0]
def sha(data):
    return hashlib.sha1(data).digest()

def verify(data):
    require(len(data)>=24 and data[:4]==b"XEX2","missing XEX2 signature")
    flags,header_size,_,sec,count=struct.unpack_from(">IIIII",data,4)
    require(flags==8,"must be a DLL, not a foreground title")
    require(header_size%4096==0 and 24<=header_size<len(data),"invalid header size")
    require(count<=64 and 24+8*count<=sec and sec+0x184<=header_size,"invalid header table")
    h=data[:header_size]
    table=dict(struct.unpack_from(">II",h,24+8*k) for k in range(count))
    require(len(table)==count,"duplicate optional header")
    for key in (0x3ff,0x10100,0x10201,0x103ff,0x20200):
        require(key in table,f"missing required header {key:x}")
    require(table[0x10201]==BASE,"unexpected image base")
    image_size=u32(h,sec+4);security_size=u32(h,sec)
    pages=u32(h,sec+0x180)
    require(0<pages<=16 and image_size==pages*PAGE,"invalid image/page count")
    require(security_size==0x184+pages*24 and sec+security_size<=header_size,"invalid security info")
    require(h[sec+0x164:sec+0x178]==sha(h[sec+0x17c:]+h[:sec+8]),"header SHA-1 mismatch")
    fmt=table[0x3ff];size=u32(h,fmt)
    require(size>=16 and (size-8)%8==0 and fmt+size<=header_size,"invalid basic compression table")
    require(struct.unpack_from(">HH",h,fmt+4)==(0,1),"expected unencrypted/basic-compressed XEX")
    image=bytearray();position=header_size
    for offset in range(fmt+8,fmt+size,8):
        stored,zero=struct.unpack_from(">II",h,offset)
        require(position+stored<=len(data) and len(image)+stored+zero<=image_size,"invalid compression block")
        image+=data[position:position+stored]+bytes(zero);position+=stored
    require(position==len(data),"trailing bytes outside compressed image")
    image+=bytes(image_size-len(image))
    expected=h[sec+0x114:sec+0x128]
    for page in range(pages):
        descriptor=h[sec+0x184+24*page:sec+0x184+24*(page+1)]
        value=u32(descriptor,0)
        require(value>>4==1 and value&15 in (1,2,3),"invalid page descriptor")
        require(sha(image[page*PAGE:(page+1)*PAGE]+descriptor)==expected,"image page SHA-1 mismatch")
        expected=descriptor[4:]
    require(expected==bytes(20),"page digest chain does not terminate")
    require(image[:4]==b"MZ\x90\x00","invalid embedded DOS header")
    pe=struct.unpack_from("<I",image,0x3c)[0]
    require(pe+24+224<=len(image) and image[pe:pe+4]==b"PE\0\0","invalid embedded PE")
    machine,nsections,_,_,_,optbytes,characteristics=struct.unpack_from("<HHIIIHH",image,pe+4)
    require(machine==0x1f2 and optbytes==224 and characteristics&0x2000,"PE is not a PowerPC BE DLL")
    optional=pe+24
    entry=BASE+struct.unpack_from("<I",image,optional+16)[0]
    require(entry==table[0x10100] and BASE<=entry<BASE+image_size,"entry point mismatch")
    require(struct.unpack_from("<I",image,optional+28)[0]==BASE,"PE/XEX image base mismatch")
    executable=False
    for k in range(nsections):
        section=optional+224+40*k
        _,vsize,rva,_,_,_,_,_,_,bits=struct.unpack_from("<8sIIIIIIHHI",image,section)
        require(rva+vsize<=image_size,"PE section outside image")
        if bits&0x20000000 and BASE+rva<=entry<BASE+rva+vsize:executable=True
    require(executable,"entry is outside executable memory")
    imports=table[0x103ff];isize,string_size,nlibs=struct.unpack_from(">III",h,imports)
    require(imports+isize<=header_size and nlibs==2 and string_size<=isize-12,"invalid imports")
    names=[n.decode("ascii") for n in h[imports+12:imports+12+string_size].split(b"\0") if n]
    require(names==["xam.xex","xboxkrnl.exe"],"unexpected import libraries")
    cursor=imports+12+string_size;expected=h[sec+0x12c:sec+0x140];records=0
    for index in range(nlibs):
        length=u32(h,cursor);require(length>=40 and cursor+length<=imports+isize,"invalid import record")
        record=h[cursor:cursor+length];require(sha(record[4:])==expected,"import digest mismatch");expected=record[4:24]
        library_index,nrecords=struct.unpack_from(">HH",record,36)
        require(library_index==index and length==40+4*nrecords,"import record count mismatch")
        for offset in range(40,length,4):
            address=u32(record,offset);require(BASE<=address<BASE+image_size-3,"import slot outside image")
            word=u32(image,address-BASE);require((word>>16)&255==index and word>>24 in (0,1),"invalid import slot")
            records+=1
        cursor+=length
    require(cursor==imports+isize and expected==bytes(20),"import chain does not terminate")
    return {"format":"XEX2 system DLL","image_base":f"0x{BASE:08X}","entry":f"0x{entry:08X}",
            "image_bytes":image_size,"file_bytes":len(data),"import_records":records,"hardware_tested":False}

if __name__=="__main__":
    try:
        print(json.dumps(verify(Path(sys.argv[1]).read_bytes()),indent=2))
    except (ValueError,struct.error,IndexError,UnicodeError) as error:
        sys.exit(f"XEX verification failed: {error}")
