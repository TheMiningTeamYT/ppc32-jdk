package org.omg.IOP;


/**
* org/omg/IOP/TAG_INTERNET_IOP.java .
* Generated by the IDL-to-Java compiler (portable), version "3.2"
* from /var/lib/jenkins/ws/workspace/zulu8/linux/ppchf/c1/build/generic/ca/release/crossbuild/zulu8-emb-dev/corba/src/share/classes/org/omg/PortableInterceptor/IOP.idl
* Monday, August 3, 2020 4:28:24 PM MSK
*/

public interface TAG_INTERNET_IOP
{

  /**
       * Identifies profiles that 
       * support the Internet Inter-ORB Protocol. The <code>ProfileBody</code>
       * of this profile contains a CDR encapsulation of a structure 
       * containing addressing and object identification information used by 
       * IIOP. Version 1.1 of the <code>TAG_INTERNET_IOP</code> profile 
       * also includes an array of TaggedComponent objects that can 
       * contain additional information supporting optional IIOP features, 
       * ORB services such as security, and future protocol extensions. 
       * <p>
       * Protocols other than IIOP (such as ESIOPs and other GIOPs) can share 
       * profile information (such as object identity or security 
       * information) with IIOP by encoding their additional profile information 
       * as components in the <code>TAG_INTERNET_IOP</code> profile. All 
       * <code>TAG_INTERNET_IOP</code> profiles support IIOP, regardless of 
       * whether they also support additional protocols. Interoperable 
       * ORBs are not required to create or understand any other profile, 
       * nor are they required to create or understand any of the components 
       * defined for other protocols that might share the 
       * <code>TAG_INTERNET_IOP</code> profile with IIOP. 
       * <p>
       * The <code>profile_data</code> for the <code>TAG_INTERNET_IOP</code> 
       * profile is a CDR encapsulation of the <code>IIOP.ProfileBody_1_1</code>
       * type.
       */
  public static final int value = (int)(0L);
}