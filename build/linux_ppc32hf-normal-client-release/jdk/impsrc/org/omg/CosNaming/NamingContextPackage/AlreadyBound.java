package org.omg.CosNaming.NamingContextPackage;


/**
* org/omg/CosNaming/NamingContextPackage/AlreadyBound.java .
* Generated by the IDL-to-Java compiler (portable), version "3.2"
* from /var/lib/jenkins/ws/workspace/zulu8/linux/ppchf/c1/build/generic/ca/release/crossbuild/zulu8-emb-dev/corba/src/share/classes/org/omg/CosNaming/nameservice.idl
* Monday, August 3, 2020 4:28:24 PM MSK
*/

public final class AlreadyBound extends org.omg.CORBA.UserException
{

  public AlreadyBound ()
  {
    super(AlreadyBoundHelper.id());
  } // ctor


  public AlreadyBound (String $reason)
  {
    super(AlreadyBoundHelper.id() + "  " + $reason);
  } // ctor

} // class AlreadyBound
