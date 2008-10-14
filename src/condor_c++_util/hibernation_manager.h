/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _HIBERNATION_MANAGER_H_
#define _HIBERNATION_MANAGER_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "extArray.h"
#include "hibernator.h"
#include "network_adapter.h"

/***************************************************************
 * HibernationManager class
 ***************************************************************/

class HibernationManager
{

public:

	/** @name Instantiation.
		*/
	//@{
	
	/// Constructor
	HibernationManager ( void ) throw ();
	
	/// Destructor
	virtual ~HibernationManager ( void ) throw ();
	
	//@}

	/** @name Power management.
		Basic checks to determined if we want to use the power
		management capabilities of the OS.
		*/
	//@{


	/** Add an interface for the hibernation manager to monitor
		@param The interface to add
		@return true:suceess; false:failed
	*/
	bool addInterface( NetworkAdapterBase & );

	//@{

    /** Set which hibernation level the computer should enter
        @param the hibernation state to place machine into
        @return true if the internal state ha changed; otherwise, false.
        @see doHibernate
        @see canHibernate
        @see wantsHibernate
        @see canWake
        */
    bool setState ( HibernatorBase::SLEEP_STATE state );

	/** Signals the OS to enter hibernation.
		@return true if the machine will enter hibernation; otherwise, false.
		@see canHibernate
		@see wantsHibernate
        @see canWake
		*/
	bool doHibernate () const;

	/** Determines if the power manager is capable of hibernating the machine.
		@return true if the machine can be hibernated; otherwise, false.
		@see doHibernate
		@see wantsHibernate
        @see canWake
		*/
	bool canHibernate ( void ) const;

    /** Determines if the network adapter is capable of waking the machine.
		@return true if the machine can be woken; otherwise, false.
		@see doHibernate
		@see canHibernate
        @see wantsHibernate
		*/
	bool canWake ( void ) const;

	/** Determines if the user wants the machine to hibernate
	    (based on the configuration file).
		@return true if the user wants the machine to enter
		        hibernation; otherwise, false.
		@see doHibernate
		@see canHibernate
        @see canWake
		*/
	bool wantsHibernate ( void ) const;

	//@}

	/** Get the time interval for checking the HIBERNATE expression.
		@return interval in seconds, or zero if no power management is
		to be used.
		@see doHibernate
		@see canHibernate
		@see wantsHibernate
		*/
	int getHibernateCheckInterval ( void ) const;

	/** Reset all the internal values based on what the values in the
	    configuration file.
		*/
	void update ( void );

    /** Published the hibernation manager's information into
        the given ad */
    void publish ( ClassAd &ad );

private:

	ExtArray<NetworkAdapterBase *>	 m_adapters;
	NetworkAdapterBase			 	*m_primary_adapter;
	HibernatorBase					*m_hibernator;
	int								 m_interval;	
    HibernatorBase::SLEEP_STATE		 m_state;	

};

#endif // _HIBERNATION_MANAGER_H_
