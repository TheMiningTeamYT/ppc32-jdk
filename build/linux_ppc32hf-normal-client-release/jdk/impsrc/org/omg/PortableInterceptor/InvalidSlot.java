package org.omg.PortableInterceptor;


/**
* org/omg/PortableInterceptor/InvalidSlot.java .
* Generated by the IDL-to-Java compiler (portable), version "3.2"
* from /var/lib/jenkins/ws/workspace/zulu8/linux/ppchf/c1/build/generic/ca/release/crossbuild/zulu8-emb-dev/corba/src/share/classes/org/omg/PortableInterceptor/Interceptors.idl
* Monday, August 3, 2020 4:28:24 PM MSK
*/

public final class InvalidSlot extends org.omg.CORBA.UserException
{

  public InvalidSlot ()
  {
    super(InvalidSlotHelper.id());
  } // ctor


  public InvalidSlot (String $reason)
  {
    super(InvalidSlotHelper.id() + "  " + $reason);
  } // ctor

} // class InvalidSlot
