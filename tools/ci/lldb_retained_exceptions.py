"""Read-only exception tracing for an authenticated retained test executable.

Uses LLDB SB APIs and process memory reads; never evaluates inferior functions,
changes registers, suppresses exceptions or alters the executable. libc++ error
message layout is admitted only after the native control probe verifies it.
"""
import json
import os
from pathlib import Path
import lldb

EVENTS=[]
OBJECTS={}
BREAKPOINTS=[]
ERRORS=[]
STANDARD_MESSAGES={"St16invalid_argument","St13runtime_error","St12length_error","St12out_of_range","St11logic_error","St12domain_error","St14overflow_error","St15underflow_error"}

def emit(event):
    if len(EVENTS)>=20000:raise RuntimeError("exception event limit exceeded")
    EVENTS.append(event)
    path=Path(os.environ['QPX_EXCEPTION_LOG'])
    with path.open('a',encoding='utf-8') as stream:stream.write(json.dumps(event,sort_keys=True)+'\n')

def register(frame,index):
    triple=frame.GetThread().GetProcess().GetTarget().GetTriple()
    names=('x0','x1') if triple.startswith(('arm64','aarch64')) else ('rdi','rsi')
    value=frame.FindRegister(names[index])
    if not value.IsValid():raise RuntimeError('missing ABI argument register '+names[index])
    return value.GetValueAsUnsigned()

def pointer(process,address):
    error=lldb.SBError();value=process.ReadPointerFromMemory(address,error)
    if not error.Success():raise RuntimeError('unreadable pointer at '+hex(address))
    return value

def cstring(process,address):
    error=lldb.SBError();value=process.ReadCStringFromMemory(address,1024,error)
    if not error.Success():raise RuntimeError('unreadable string at '+hex(address))
    return value

def stack(frame):
    thread=frame.GetThread();target=thread.GetProcess().GetTarget();out=[]
    for index in range(min(thread.GetNumFrames(),24)):
        current=thread.GetFrameAtIndex(index);symbol=current.GetSymbol();start=symbol.GetStartAddress().GetLoadAddress(target) if symbol.IsValid() else lldb.LLDB_INVALID_ADDRESS
        pc=current.GetPC();line=current.GetLineEntry()
        out.append(dict(index=index,pc=hex(pc),function=current.GetFunctionName(),module=current.GetModule().GetFileSpec().GetFilename(),offset=hex(pc-start) if start!=lldb.LLDB_INVALID_ADDRESS else None,file=str(line.GetFileSpec()) if line.IsValid() else None,line=line.GetLine() if line.IsValid() else None))
    return out

def on_throw(frame,bp_loc,internal_dict):
    try:
        process=frame.GetThread().GetProcess();obj=register(frame,0);typeinfo=register(frame,1);size=process.GetTarget().GetAddressByteSize()
        name=cstring(process,pointer(process,typeinfo+size));message=None
        if name in STANDARD_MESSAGES:message=cstring(process,pointer(process,obj+size))
        event=dict(event='throw',id=len(EVENTS)+1,thread=frame.GetThread().GetThreadID(),object=hex(obj),typeinfo=hex(typeinfo),type=name,message=message,stack=stack(frame))
        OBJECTS[obj]=event;emit(event)
    except Exception as error:
        ERRORS.append(str(error));emit(dict(event='observer_error',callback='throw',error=str(error),stack=stack(frame)))
    return False

def on_catch(frame,bp_loc,internal_dict):
    try:
        argument=register(frame,0);size=frame.GetThread().GetProcess().GetTarget().GetAddressByteSize()
        # Itanium unwind header is four pointer-sized words on both target ABIs.
        # The real native probe must establish this association before test use.
        obj=argument+4*size;thrown=OBJECTS.get(obj)
        emit(dict(event='catch',thread=frame.GetThread().GetThreadID(),argument=hex(argument),candidate_object=hex(obj),matched_throw=thrown['id'] if thrown else None,type=thrown['type'] if thrown else None,message=thrown['message'] if thrown else None,stack=stack(frame)))
    except Exception as error:
        ERRORS.append(str(error));emit(dict(event='observer_error',callback='catch',error=str(error),stack=stack(frame)))
    return False

def summary(debugger,command,result,internal_dict):
    process=debugger.GetSelectedTarget().GetProcess()
    data=dict(schema='qpx.retained-exception-trace/1',process_state=process.GetState(),exited=process.GetState()==lldb.eStateExited,exit_status=process.GetExitStatus(),exit_description=process.GetExitDescription(),events=len(EVENTS),throws=sum(e['event']=='throw' for e in EVENTS),catches=sum(e['event']=='catch' for e in EVENTS),observer_errors=ERRORS,breakpoints=[dict(name=name,locations=bp.GetNumLocations(),hits=bp.GetHitCount()) for name,bp in BREAKPOINTS],series_events=[e for e in EVENTS if e.get('message')=='raw grid source series changed'])
    Path(os.environ['QPX_EXCEPTION_SUMMARY']).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
    result.PutCString(json.dumps({k:v for k,v in data.items() if k!='series_events'},sort_keys=True))

def __lldb_init_module(debugger,internal_dict):
    target=debugger.GetSelectedTarget()
    for name,callback in [('__cxa_throw','on_throw'),('__cxa_begin_catch','on_catch')]:
        bp=target.BreakpointCreateByName(name);bp.SetScriptCallbackFunction(__name__+'.'+callback);BREAKPOINTS.append((name,bp))
    debugger.HandleCommand('command script add -f '+__name__+'.summary retained_summary')
