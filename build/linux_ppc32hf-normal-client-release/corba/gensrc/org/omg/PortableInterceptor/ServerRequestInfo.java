package org.omg.PortableInterceptor;


/**
* org/omg/PortableInterceptor/ServerRequestInfo.java .
* Generated by the IDL-to-Java compiler (portable), version "3.2"
* from /var/lib/jenkins/ws/workspace/zulu8/linux/ppchf/c1/build/generic/ca/release/crossbuild/zulu8-emb-dev/corba/src/share/classes/org/omg/PortableInterceptor/Interceptors.idl
* Monday, August 3, 2020 4:28:24 PM MSK
*/


/**
   * Request Information, accessible to server-side request interceptors.
   * <p>
   * Some attributes and operations on <code>ServerRequestInfo</code> are not 
   * valid at all interception points.  The following table shows the validity 
   * of each attribute or operation.  If it is not valid, attempting to access 
   * it will result in a <code>BAD_INV_ORDER</code> being thrown with a 
   * standard minor code of 14.
   * <p>
   *
   *
   * <table border=1 summary="Shows the validity of each attribute or operation">
   *   <thead>
   *     <tr>
   *       <th>&nbsp;</th>
   *       <th id="rec_req_ser_con" valign="bottom">receive_request_<br>service_contexts</th>
   *       <th id="rec_req"  valign="bottom">receive_request</th>
   *       <th id="send_rep" valign="bottom">send_reply</th>
   *       <th id="send_exc" valign="bottom">send_exception</th>
   *       <th id="send_oth" valign="bottom">send_other</th>
   *     </tr>
   *   </thead>
   *  <tbody>
   *
   *
   * <tr>
   *   <td id="ri" colspan=6><i>Inherited from RequestInfo:</i></td>
   * </tr>
   *
   * <tr><th id="req_id"><p align="left">request_id</p></th>
   * <td headers="ri req_id rec_req_ser_con">yes</td> 
   * <td headers="ri req_id rec_req">yes</td> 
   * <td headers="ri req_id send_rep">yes</td> 
   * <td headers="ri req_id send_exc">yes</td> 
   * <td headers="ri req_id send_oth">yes</td></tr>
   *
   * <tr><th id="op"><p align="left">operation</p></th>
   * <td headers="ri op rec_req_ser_con">yes</td> 
   * <td headers="ri op rec_req">yes</td> 
   * <td headers="ri op send_rep">yes</td> 
   * <td headers="ri op send_exc">yes</td> 
   * <td headers="ri op send_oth">yes</td></tr>
   *
   * <tr><th id="args"><p align="left">arguments</p></th>
   * <td headers="ri args rec_req_ser_con">no </td> 
   * <td headers="ri args rec_req">yes<sub>1</sub></td>
   * <td headers="ri args send_rep">yes</td> 
   * <td headers="ri args send_exc">no<sub>2</sub></td>
   * <td headers="ri args send_oth">no<sub>2</sub>
   * </td></tr>
   *
   * <tr><th id="exps"><p align="left">exceptions</p></th>
   * <td headers="ri exps rec_req_ser_con">no </td> 
   * <td headers="ri exps rec_req">yes</td> 
   * <td headers="ri exps send_rep">yes</td> 
   * <td headers="ri exps send_exc">yes</td> 
   * <td headers="ri exps send_oth">yes</td></tr>
   *
   * <tr><th id="contexts"><p align="left">contexts</p></th>
   * <td headers="ri contexts rec_req_ser_con">no </td> 
   * <td headers="ri contexts rec_req">yes</td> 
   * <td headers="ri contexts send_rep">yes</td> 
   * <td headers="ri contexts send_exc">yes</td> 
   * <td headers="ri contexts send_oth">yes</td></tr>
   *
   * <tr><th id="op_con"><p align="left">operation_context</p></th>
   * <td headers="ri op_con rec_req_ser_con">no </td> 
   * <td headers="ri op_con rec_req">yes</td> 
   * <td headers="ri op_con send_rep">yes</td> 
   * <td headers="ri op_con send_exc">no </td> 
   * <td headers="ri op_con send_oth">no </td>
   * </tr>
   * 
   * <tr><th id="result"><p align="left">result</p></th>
   * <td headers="ri result rec_req_ser_con">no </td> 
   * <td headers="ri result rec_req">no </td> 
   * <td headers="ri result send_rep">yes</td> 
   * <td headers="ri result send_exc">no </td> 
   * <td headers="ri result send_oth">no </td>
   * </tr>
   *
   * <tr><th id="res_ex"><p align="left">response_expected</p></th>
   * <td headers="ri res_ex rec_req_ser_con">yes</td> 
   * <td headers="ri res_ex rec_req">yes</td> 
   * <td headers="ri res_ex send_rep">yes</td> 
   * <td headers="ri res_ex send_exc">yes</td> 
   * <td headers="ri res_ex send_oth">yes</td></tr>
   *
   * <tr><th id="syn_scp"><p align="left">sync_scope</p></th>
   * <td headers="ri syn_scp rec_req_ser_con">yes</td> 
   * <td headers="ri syn_scp rec_req">yes</td> 
   * <td headers="ri syn_scp send_rep">yes</td> 
   * <td headers="ri syn_scp send_exc">yes</td> 
   * <td headers="ri syn_scp send_oth">yes</td></tr>
   * 
   *    <tr><td><b>request_id</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>operation</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>arguments</b></td>
   *    <td>no </td> <td>yes<sub>1</sub</td> 
   *                              <td>yes</td> <td>no<sub>2</sub></td> 
   *                                                        <td>no<sub>2</sub>
   *                                                        </td></tr>
   * 
   *    <tr><td><b>exceptions</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>contexts</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>operation_context</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>no </td> <td>no </td></tr>
   * 
   *    <tr><td><b>result</b></td>
   *    <td>no </td> <td>no </td> <td>yes</td> <td>no </td> <td>no </td></tr>
   * 
   *    <tr><td><b>response_expected</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>sync_scope</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>reply_status</b></td>
   *    <td>no </td> <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>forward_reference</b></td>
   *    <td>no </td> <td>no </td> <td>no </td> <td>no </td> <td>yes<sub>2</sub>
   * 								</td></tr>
   * 
   *    <tr><td><b>get_slot</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>get_request_service_context</b></td>
   *    <td>yes</td> <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>get_reply_service_context</b></td>
   *    <td>no </td> <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr>
   *      <td colspan=6><i>ServerRequestInfo-specific:</i></td>
   *    </tr>
   * 
   *    <tr><td><b>sending_exception</b></td>
   *    <td>no </td> <td>no </td> <td>no </td> <td>yes</td> <td>no </td></tr>
   * 
   *    <tr><td><b>object_id</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes<sub>3</sub></td> 
   * 						       <td>yes<sub>3</sub>
   * 						                </td></tr>
   * 
   *    <tr><td><b>adapter_id</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes<sub>3</sub></td> 
   * 						       <td>yes<sub>3</sub>
   * 								</td></tr>
   * 
   *    <tr><td><b>server_id</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   *
   *    <tr><td><b>orb_id</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   *
   *    <tr><td><b>adapter_name</b></td>
   *    <td>no </td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   *
   *    <tr><td><b>target_most_derived_interface</b></td>
   *    <td>no </td> <td>yes</td> <td>no<sub>4</sub></td> 
   * 					  <td>no<sub>4</sub></td> 
   * 						       <td>no<sub>4</sub>
   * 							       </td></tr>
   * 
   *    <tr><td><b>get_server_policy</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>set_slot</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   * 
   *    <tr><td><b>target_is_a</b></td>
   *    <td>no </td> <td>yes</td> <td>no<sub>4</sub></td> 
   * 					  <td>no<sub>4</sub></td> 
   * 						       <td>no<sub>4</sub>
   * 							       </td></tr>
   * 
   *    <tr><td><b>add_reply_service_context</b></td>
   *    <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td> <td>yes</td></tr>
   *   </tbody>
   * </table>
   *
   * <ol>
   *   <li>When <code>ServerRequestInfo</code> is passed to 
   *       <code>receive_request</code>, there is an entry in the list for 
   *       every argument, whether in, inout, or out. But only the in and 
   *       inout arguments will be available.</li>
   *   <li>If the <code>reply_status</code> attribute is not 
   *       <code>LOCATION_FORWARD</code>, accessing this attribute will throw
   *       <code>BAD_INV_ORDER</code> with a standard minor code of 14.</li>
   *   <li>If the servant locator caused a location forward, or thrown an 
   *       exception, this attribute/operation may not be available in this 
   *       interception point. <code>NO_RESOURCES</code> with a standard minor 
   *       code of 1 will be thrown if it is not available.</li>
   *   <li>The operation is not available in this interception point because 
   *       the necessary information requires access to the target object's 
   *       servant, which may no longer be available to the ORB. For example, 
   *       if the object's adapter is a POA that uses a 
   *       <code>ServantLocator</code>, then the ORB invokes the interception 
   *       point after it calls <code>ServantLocator.postinvoke()</code></li>.
   * </ol>
   *
   * @see ServerRequestInterceptor
   */
public interface ServerRequestInfo extends ServerRequestInfoOperations, org.omg.PortableInterceptor.RequestInfo, org.omg.CORBA.portable.IDLEntity 
{
} // interface ServerRequestInfo
