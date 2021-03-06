/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_linguistic.hxx"

#include <tools/debug.hxx>

#include <com/sun/star/linguistic2/LinguServiceEvent.hpp>
#include <com/sun/star/linguistic2/LinguServiceEventFlags.hpp>
#include <com/sun/star/linguistic2/XLinguServiceEventListener.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <osl/mutex.hxx>

#include <linguistic/misc.hxx>
#include <linguistic/lngprops.hxx>

#include <linguistic/lngprophelp.hxx>


//using namespace utl;
using namespace osl;
using namespace rtl;
using namespace com::sun::star;
using namespace com::sun::star::beans;
using namespace com::sun::star::lang;
using namespace com::sun::star::uno;
using namespace com::sun::star::linguistic2;
using namespace linguistic;

namespace linguistic
{

///////////////////////////////////////////////////////////////////////////

static const char *aCH[] =
{
	UPN_IS_IGNORE_CONTROL_CHARACTERS,
	UPN_IS_USE_DICTIONARY_LIST,
};

static int nCHCount = sizeof(aCH) / sizeof(aCH[0]);


PropertyChgHelper::PropertyChgHelper(
		const Reference< XInterface > &rxSource,
		Reference< XPropertySet > &rxPropSet,
		int nAllowedEvents ) :
    PropertyChgHelperBase(),
    aPropNames          (nCHCount),
    xMyEvtObj           (rxSource),
    aLngSvcEvtListeners (GetLinguMutex()),
    xPropSet            (rxPropSet),
    nEvtFlags           (nAllowedEvents)
{
	OUString *pName = aPropNames.getArray();
	for (sal_Int32 i = 0;  i < nCHCount;  ++i)
	{
		pName[i] = A2OU( aCH[i] );
	}

	SetDefaultValues();
}


PropertyChgHelper::PropertyChgHelper( const PropertyChgHelper &rHelper ) :
    PropertyChgHelperBase(),
    aLngSvcEvtListeners (GetLinguMutex())
{
	RemoveAsPropListener();
    aPropNames  = rHelper.aPropNames;
    xMyEvtObj   = rHelper.xMyEvtObj;
    xPropSet    = rHelper.xPropSet;
    nEvtFlags   = rHelper.nEvtFlags;
    AddAsPropListener();

	SetDefaultValues();
	GetCurrentValues();
}


PropertyChgHelper::~PropertyChgHelper()
{
}


void PropertyChgHelper::AddPropNames( const char *pNewNames[], sal_Int32 nCount )
{
	if (pNewNames && nCount)
	{
		sal_Int32 nLen = GetPropNames().getLength();
		GetPropNames().realloc( nLen + nCount );
		OUString *pName = GetPropNames().getArray();
		for (sal_Int32 i = 0;  i < nCount;  ++i)
		{
			pName[ nLen + i ] = A2OU( pNewNames[ i ] );
		}
	}
}


void PropertyChgHelper::SetDefaultValues()
{
	bResIsIgnoreControlCharacters	= bIsIgnoreControlCharacters	= sal_True;
	bResIsUseDictionaryList			= bIsUseDictionaryList			= sal_True;
}


void PropertyChgHelper::GetCurrentValues()
{
	sal_Int32 nLen = GetPropNames().getLength();
	if (GetPropSet().is() && nLen)
	{
		const OUString *pPropName = GetPropNames().getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			sal_Bool *pbVal		= NULL,
				 *pbResVal	= NULL;
			
            if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_IS_IGNORE_CONTROL_CHARACTERS ) ))
			{
				pbVal	 = &bIsIgnoreControlCharacters;
				pbResVal = &bResIsIgnoreControlCharacters;
			}
            else if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_IS_USE_DICTIONARY_LIST ) ))
			{
				pbVal	 = &bIsUseDictionaryList;
				pbResVal = &bResIsUseDictionaryList;
			}

			if (pbVal && pbResVal)
			{
				GetPropSet()->getPropertyValue( pPropName[i] ) >>= *pbVal;
				*pbResVal = *pbVal;
			}
		}
	}
}


void PropertyChgHelper::SetTmpPropVals( const PropertyValues &rPropVals )
{
	// return value is default value unless there is an explicitly supplied
	// temporary value
	bResIsIgnoreControlCharacters	= bIsIgnoreControlCharacters;
	bResIsUseDictionaryList			= bIsUseDictionaryList;
	//
	sal_Int32 nLen = rPropVals.getLength();
	if (nLen)
	{
		const PropertyValue *pVal = rPropVals.getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			sal_Bool  *pbResVal = NULL;
			switch (pVal[i].Handle)
			{
				case UPH_IS_IGNORE_CONTROL_CHARACTERS : 
						pbResVal = &bResIsIgnoreControlCharacters; break;
				case UPH_IS_USE_DICTIONARY_LIST		: 
						pbResVal = &bResIsUseDictionaryList; break;
				default:
						;
                    //DBG_ASSERT( 0, "unknown property" );
			}
			if (pbResVal)
				pVal[i].Value >>= *pbResVal;
		}
	}
}


sal_Bool PropertyChgHelper::propertyChange_Impl( const PropertyChangeEvent& rEvt )
{
	sal_Bool bRes = sal_False;

	if (GetPropSet().is()  &&  rEvt.Source == GetPropSet())
	{
		sal_Int16 nLngSvcFlags = (nEvtFlags & AE_HYPHENATOR) ?
					LinguServiceEventFlags::HYPHENATE_AGAIN : 0;
		sal_Bool bSCWA = sal_False,	// SPELL_CORRECT_WORDS_AGAIN ?
			 bSWWA = sal_False;	// SPELL_WRONG_WORDS_AGAIN ?

		sal_Bool  *pbVal = NULL;
		switch (rEvt.PropertyHandle)
		{
			case UPH_IS_IGNORE_CONTROL_CHARACTERS :
			{
				pbVal = &bIsIgnoreControlCharacters;
				nLngSvcFlags = 0;
				break;
			}
			case UPH_IS_USE_DICTIONARY_LIST		  :
			{
				pbVal = &bIsUseDictionaryList;
				bSCWA = bSWWA = sal_True;
				break;
			}
			default:
			{
				bRes = sal_False;
                //DBG_ASSERT( 0, "unknown property" );
			}
		}
		if (pbVal)
			rEvt.NewValue >>= *pbVal;

		bRes = 0 != pbVal;	// sth changed?
		if (bRes)
		{
			sal_Bool bSpellEvts = (nEvtFlags & AE_SPELLCHECKER) ? sal_True : sal_False;
			if (bSCWA && bSpellEvts)
				nLngSvcFlags |= LinguServiceEventFlags::SPELL_CORRECT_WORDS_AGAIN;
			if (bSWWA && bSpellEvts)
				nLngSvcFlags |= LinguServiceEventFlags::SPELL_WRONG_WORDS_AGAIN;
			if (nLngSvcFlags)
			{
				LinguServiceEvent aEvt( GetEvtObj(), nLngSvcFlags );
				LaunchEvent( aEvt );
			}
		}
	}

	return bRes;
}


void SAL_CALL
	PropertyChgHelper::propertyChange( const PropertyChangeEvent& rEvt )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );
	propertyChange_Impl( rEvt );
}


void PropertyChgHelper::AddAsPropListener()
{
	if (xPropSet.is())
	{
		sal_Int32 nLen = aPropNames.getLength();
		const OUString *pPropName = aPropNames.getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			if (pPropName[i].getLength())
				xPropSet->addPropertyChangeListener( pPropName[i], this );
		}
	}
}

void PropertyChgHelper::RemoveAsPropListener()
{
	if (xPropSet.is())
	{
		sal_Int32 nLen = aPropNames.getLength();
		const OUString *pPropName = aPropNames.getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			if (pPropName[i].getLength())
				xPropSet->removePropertyChangeListener( pPropName[i], this );
		}
	}
}


void PropertyChgHelper::LaunchEvent( const LinguServiceEvent &rEvt )
{
	cppu::OInterfaceIteratorHelper aIt( aLngSvcEvtListeners );
	while (aIt.hasMoreElements())
	{
		Reference< XLinguServiceEventListener > xRef( aIt.next(), UNO_QUERY );
		if (xRef.is())
			xRef->processLinguServiceEvent( rEvt );
	}
}


void SAL_CALL PropertyChgHelper::disposing( const EventObject& rSource )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );
	if (rSource.Source == xPropSet)
	{
		RemoveAsPropListener();
		xPropSet = NULL;
		aPropNames.realloc( 0 );
	}
}


sal_Bool SAL_CALL
	PropertyChgHelper::addLinguServiceEventListener(
			const Reference< XLinguServiceEventListener >& rxListener )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );

	sal_Bool bRes = sal_False;
	if (rxListener.is())
	{
		sal_Int32	nCount = aLngSvcEvtListeners.getLength();
		bRes = aLngSvcEvtListeners.addInterface( rxListener ) != nCount;
	}
	return bRes;
}


sal_Bool SAL_CALL
	PropertyChgHelper::removeLinguServiceEventListener(
			const Reference< XLinguServiceEventListener >& rxListener )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );

	sal_Bool bRes = sal_False;
	if (rxListener.is())
	{
		sal_Int32	nCount = aLngSvcEvtListeners.getLength();
		bRes = aLngSvcEvtListeners.removeInterface( rxListener ) != nCount;
	}
	return bRes;
}

///////////////////////////////////////////////////////////////////////////
	

PropertyHelper_Thes::PropertyHelper_Thes(
		const Reference< XInterface > &rxSource,
		Reference< XPropertySet > &rxPropSet ) :
	PropertyChgHelper	( rxSource, rxPropSet, 0 )
{
	SetDefaultValues();
	GetCurrentValues();
}


PropertyHelper_Thes::~PropertyHelper_Thes()
{
}


void SAL_CALL
	PropertyHelper_Thes::propertyChange( const PropertyChangeEvent& rEvt )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );
    PropertyChgHelper::propertyChange_Impl( rEvt );
}


///////////////////////////////////////////////////////////////////////////

// list of properties from the property set to be used
// and listened to
static const char *aSP[] =
{
	UPN_IS_SPELL_UPPER_CASE,
	UPN_IS_SPELL_WITH_DIGITS,
    UPN_IS_SPELL_CAPITALIZATION
};


PropertyHelper_Spell::PropertyHelper_Spell(
		const Reference< XInterface > & rxSource,
		Reference< XPropertySet > &rxPropSet ) :
	PropertyChgHelper	( rxSource, rxPropSet, AE_SPELLCHECKER )
{
	AddPropNames( aSP, sizeof(aSP) / sizeof(aSP[0]) );
	SetDefaultValues();
	GetCurrentValues();

    nResMaxNumberOfSuggestions = GetDefaultNumberOfSuggestions();
}


PropertyHelper_Spell::~PropertyHelper_Spell()
{
}


void PropertyHelper_Spell::SetDefaultValues()
{
	PropertyChgHelper::SetDefaultValues();

	bResIsSpellUpperCase		= bIsSpellUpperCase			= sal_False;
	bResIsSpellWithDigits		= bIsSpellWithDigits		= sal_False;
	bResIsSpellCapitalization	= bIsSpellCapitalization	= sal_True;
}


void PropertyHelper_Spell::GetCurrentValues()
{
	PropertyChgHelper::GetCurrentValues();

	sal_Int32 nLen = GetPropNames().getLength();
	if (GetPropSet().is() && nLen)
	{
		const OUString *pPropName = GetPropNames().getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			sal_Bool *pbVal		= NULL,
				 *pbResVal	= NULL;
			
            if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_IS_SPELL_UPPER_CASE ) ))
			{
				pbVal	 = &bIsSpellUpperCase;
				pbResVal = &bResIsSpellUpperCase;
			}
            else if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_IS_SPELL_WITH_DIGITS ) ))
			{
				pbVal	 = &bIsSpellWithDigits;
				pbResVal = &bResIsSpellWithDigits;
			}
            else if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_IS_SPELL_CAPITALIZATION ) ))
			{
				pbVal	 = &bIsSpellCapitalization;
				pbResVal = &bResIsSpellCapitalization;
			}

			if (pbVal && pbResVal)
			{
				GetPropSet()->getPropertyValue( pPropName[i] ) >>= *pbVal;
				*pbResVal = *pbVal;
			}
		}
	}
}


sal_Bool PropertyHelper_Spell::propertyChange_Impl( const PropertyChangeEvent& rEvt )
{
	sal_Bool bRes = PropertyChgHelper::propertyChange_Impl( rEvt );

	if (!bRes  &&  GetPropSet().is()  &&  rEvt.Source == GetPropSet())
	{
		sal_Int16 nLngSvcFlags = 0;
		sal_Bool bSCWA = sal_False,	// SPELL_CORRECT_WORDS_AGAIN ?
			 bSWWA = sal_False;	// SPELL_WRONG_WORDS_AGAIN ?

		sal_Bool *pbVal = NULL;
		switch (rEvt.PropertyHandle)
		{
			case UPH_IS_SPELL_UPPER_CASE		  :
			{
				pbVal = &bIsSpellUpperCase;
				bSCWA = sal_False == *pbVal;	// sal_False->sal_True change?
				bSWWA = !bSCWA;				// sal_True->sal_False change?
				break;
			}
			case UPH_IS_SPELL_WITH_DIGITS		  :
			{
				pbVal = &bIsSpellWithDigits;
				bSCWA = sal_False == *pbVal;	// sal_False->sal_True change?
				bSWWA = !bSCWA;				// sal_True->sal_False change?
				break;
			}
			case UPH_IS_SPELL_CAPITALIZATION	  :
			{
				pbVal = &bIsSpellCapitalization;
				bSCWA = sal_False == *pbVal;	// sal_False->sal_True change?
				bSWWA = !bSCWA;				// sal_True->sal_False change?
				break;
			}
			default:
                DBG_ASSERT( 0, "unknown property" );
		}
		if (pbVal)
			rEvt.NewValue >>= *pbVal;

		bRes = (pbVal != 0);
		if (bRes)
		{
			if (bSCWA)
				nLngSvcFlags |= LinguServiceEventFlags::SPELL_CORRECT_WORDS_AGAIN;
			if (bSWWA)
				nLngSvcFlags |= LinguServiceEventFlags::SPELL_WRONG_WORDS_AGAIN;
			if (nLngSvcFlags)
			{
				LinguServiceEvent aEvt( GetEvtObj(), nLngSvcFlags );
				LaunchEvent( aEvt );
			}
		}
	}

	return bRes;
}


void SAL_CALL
	PropertyHelper_Spell::propertyChange( const PropertyChangeEvent& rEvt )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );
	propertyChange_Impl( rEvt );
}


void PropertyHelper_Spell::SetTmpPropVals( const PropertyValues &rPropVals )
{
	PropertyChgHelper::SetTmpPropVals( rPropVals );

	// return value is default value unless there is an explicitly supplied
	// temporary value
    nResMaxNumberOfSuggestions  = GetDefaultNumberOfSuggestions();
	bResIsSpellWithDigits		= bIsSpellWithDigits;
	bResIsSpellCapitalization	= bIsSpellCapitalization;
	//
	sal_Int32 nLen = rPropVals.getLength();
	if (nLen)
	{
		const PropertyValue *pVal = rPropVals.getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
            if (pVal[i].Name.equalsAscii( UPN_MAX_NUMBER_OF_SUGGESTIONS ))
            {
                pVal[i].Value >>= nResMaxNumberOfSuggestions;
            }
            else
            {
                sal_Bool *pbResVal = NULL;
                switch (pVal[i].Handle)
                {
                    case UPH_IS_SPELL_UPPER_CASE     : pbResVal = &bResIsSpellUpperCase; break;
                    case UPH_IS_SPELL_WITH_DIGITS    : pbResVal = &bResIsSpellWithDigits; break;
                    case UPH_IS_SPELL_CAPITALIZATION : pbResVal = &bResIsSpellCapitalization; break;
                    default:
                        DBG_ASSERT( 0, "unknown property" );
                }
                if (pbResVal)
                    pVal[i].Value >>= *pbResVal;
            }
		}
	}
}

sal_Int16 PropertyHelper_Spell::GetDefaultNumberOfSuggestions() const
{
    return 16;
}

///////////////////////////////////////////////////////////////////////////

static const char *aHP[] =
{
	UPN_HYPH_MIN_LEADING,
	UPN_HYPH_MIN_TRAILING,
	UPN_HYPH_MIN_WORD_LENGTH
};


PropertyHelper_Hyphen::PropertyHelper_Hyphen(
		const Reference< XInterface > & rxSource,
		Reference< XPropertySet > &rxPropSet ) :
	PropertyChgHelper	( rxSource, rxPropSet, AE_HYPHENATOR )
{
	AddPropNames( aHP, sizeof(aHP) / sizeof(aHP[0]) );
	SetDefaultValues();
	GetCurrentValues();
}


PropertyHelper_Hyphen::~PropertyHelper_Hyphen()
{
}


void PropertyHelper_Hyphen::SetDefaultValues()
{
	PropertyChgHelper::SetDefaultValues();

	nResHyphMinLeading	 	= nHyphMinLeading		= 2;
	nResHyphMinTrailing	 	= nHyphMinTrailing		= 2;
	nResHyphMinWordLength	= nHyphMinWordLength	= 0;
}


void PropertyHelper_Hyphen::GetCurrentValues()
{
	PropertyChgHelper::GetCurrentValues();

	sal_Int32 nLen = GetPropNames().getLength();
	if (GetPropSet().is() && nLen)
	{
		const OUString *pPropName = GetPropNames().getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			sal_Int16  *pnVal	 = NULL,
				   *pnResVal = NULL;

            if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_HYPH_MIN_LEADING ) ))
			{
				pnVal	 = &nHyphMinLeading;
				pnResVal = &nResHyphMinLeading;
			}
            else if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_HYPH_MIN_TRAILING ) ))
			{
				pnVal	 = &nHyphMinTrailing;
				pnResVal = &nResHyphMinTrailing;
			}
            else if (pPropName[i].equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( UPN_HYPH_MIN_WORD_LENGTH ) ))
			{
				pnVal	 = &nHyphMinWordLength;
				pnResVal = &nResHyphMinWordLength;
			}

			if (pnVal && pnResVal)
			{
				GetPropSet()->getPropertyValue( pPropName[i] ) >>= *pnVal;
				*pnResVal = *pnVal;
			}
		}
	}
}


sal_Bool PropertyHelper_Hyphen::propertyChange_Impl( const PropertyChangeEvent& rEvt )
{
	sal_Bool bRes = PropertyChgHelper::propertyChange_Impl( rEvt );

	if (!bRes  &&  GetPropSet().is()  &&  rEvt.Source == GetPropSet())
	{
		sal_Int16 nLngSvcFlags = LinguServiceEventFlags::HYPHENATE_AGAIN;

		sal_Int16	*pnVal = NULL;
		switch (rEvt.PropertyHandle)
		{
			case UPH_HYPH_MIN_LEADING	  : pnVal = &nHyphMinLeading; break;
			case UPH_HYPH_MIN_TRAILING	  : pnVal = &nHyphMinTrailing; break;
			case UPH_HYPH_MIN_WORD_LENGTH : pnVal = &nHyphMinWordLength; break;
			default:
                DBG_ASSERT( 0, "unknown property" );
		}
		if (pnVal)
			rEvt.NewValue >>= *pnVal;

		bRes = (pnVal != 0);
		if (bRes)
		{
			if (nLngSvcFlags)
			{
				LinguServiceEvent aEvt( GetEvtObj(), nLngSvcFlags );
				LaunchEvent( aEvt );
			}
		}
	}

	return bRes;
}


void SAL_CALL
    PropertyHelper_Hyphen::propertyChange( const PropertyChangeEvent& rEvt )
		throw(RuntimeException)
{
	MutexGuard	aGuard( GetLinguMutex() );
	propertyChange_Impl( rEvt );
}


void PropertyHelper_Hyphen::SetTmpPropVals( const PropertyValues &rPropVals )
{
	PropertyChgHelper::SetTmpPropVals( rPropVals );
	
	// return value is default value unless there is an explicitly supplied
	// temporary value
	nResHyphMinLeading	 	= nHyphMinLeading;
	nResHyphMinTrailing	 	= nHyphMinTrailing;
	nResHyphMinWordLength	= nHyphMinWordLength;
	//
	sal_Int32 nLen = rPropVals.getLength();
	if (nLen)
	{
		const PropertyValue *pVal = rPropVals.getConstArray();
		for (sal_Int32 i = 0;  i < nLen;  ++i)
		{
			sal_Int16	*pnResVal = NULL;
			switch (pVal[i].Handle)
			{
				case UPH_HYPH_MIN_LEADING	  : pnResVal = &nResHyphMinLeading; break;
				case UPH_HYPH_MIN_TRAILING	  : pnResVal = &nResHyphMinTrailing; break;
				case UPH_HYPH_MIN_WORD_LENGTH : pnResVal = &nResHyphMinWordLength; break;
				default:
                    DBG_ASSERT( 0, "unknown property" );
			}
			if (pnResVal)
				pVal[i].Value >>= *pnResVal;
		}
	}
}

///////////////////////////////////////////////////////////////////////////

}   // namespace linguistic

