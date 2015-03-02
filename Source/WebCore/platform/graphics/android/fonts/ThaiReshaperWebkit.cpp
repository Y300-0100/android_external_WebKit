/*
i * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ThaiReshaperWebkit.h"
#include <cutils/log.h>

//#define  equal(a,b) !((a)^(b))

/* Macro to check if we need reshaping at in[j] in thaiReshaper()
 * and if needed, break and let the loop handle j's consonanttype
 */
#define  checkAndBreak(c,val)	\
        if((c) == (val))	    \
            {			        \
            i = j - 1;	        \
            break;		        \
            }


///////////////////////////////////////////////////////////////////////////////
//[SISO][Saurabh] thai Reshaper - 15-07-11 starts
/**
 * Function to check if uniCode is thai unicode or not
 *//*** checked***/
enum ThaiReshapeFlag
{
	THAI_NO_RESHAPE 		= 0x0000,  /* Not need reshape */

	THAI_REMOVE_TAIL		= 0x0001,  /* Remove tail vowel of consonant */
	THAI_PULLDOWN_TAIL		= 0x0002,  /* Pulldown vowel */
	THAI_SHIFT_LEFT 		= 0x0004,  /* Shift left vowel or tone */

	THAI_COMBINE_SARAAM 	= 0x0010,  /* combine saraam */
	THAI_PULLDOWN_TONE		= 0x0020,  /* pulldown tone */
	THAI_MOVE_TONE			= 0x0030   /* Move tone position : combine_sara | pulldowntone */
};

enum ThaiCharClassValues
{
	Thai_CC_RESERVED		=  0,
	Thai_CC_CONSONANT		=  1, /* Consonant */
	Thai_CC_VOWEL			=  2, /* Dependent vowel */
	Thai_CC_TONE			=  3, /* Dependent tone */
	Thai_CC_COUNT			=  4  /* This is the number of character classes */
};

enum ThaiCharClassFlags
{
	Thai_CF_CLASS_MASK	  = 0x0000FFFF,

	Thai_CF_CONSONANT	  = 0x01000000,  /* flag to speed up comparing */
	Thai_CF_ABOVE_VOWEL   = 0x02000000,  /* flag to speed up comparing */
	Thai_CF_BELOW_VOWEL   = 0x04000000,  /* flag to speed up comparing */
	Thai_CF_POST_VOWEL	  = 0x08000000,  /* flag to speed up comparing */
	Thai_CF_DEP_TONE	  = 0x10000000,  /* flag to speed up comparing */
	Thai_CF_DOTTED_CIRCLE = 0x20000000,  /* add a dotted circle if a character with this flag is the first in a syllable */

	/* SHIFT flags */
	Thai_CF_SHIFT_LEFT	  = 0x00080000,
	Thai_CF_SHIFT_DOWN	  = 0x00040000,
	Thai_CF_SHIFT_UP	  = 0x00020000,
	Thai_CF_REMOVE_TAIL   = 0x00010000,
	Thai_CF_POS_MASK	  = 0x000f0000
};

enum
{
	Thai_xx = Thai_CC_RESERVED,
	Thai_c1 = Thai_CC_CONSONANT | Thai_CF_CONSONANT,
	Thai_c2 = Thai_CC_CONSONANT | Thai_CF_CONSONANT | Thai_CF_SHIFT_LEFT,
	Thai_c3 = Thai_CC_CONSONANT | Thai_CF_CONSONANT | Thai_CF_SHIFT_DOWN,
	Thai_c4 = Thai_CC_CONSONANT | Thai_CF_CONSONANT | Thai_CF_REMOVE_TAIL,
	Thai_da = Thai_CC_VOWEL | Thai_CF_ABOVE_VOWEL | Thai_CF_DOTTED_CIRCLE,
	Thai_db = Thai_CC_VOWEL | Thai_CF_BELOW_VOWEL | Thai_CF_DOTTED_CIRCLE,
	Thai_dr = Thai_CC_VOWEL | Thai_CF_POST_VOWEL | Thai_CF_DOTTED_CIRCLE,
	Thai_dt = Thai_CC_TONE | Thai_CF_DEP_TONE | Thai_CF_DOTTED_CIRCLE
};


typedef int ThaiCharClass;


static const ThaiCharClass ThaiCharClasses[] =
{
	Thai_xx, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, /* 0E00 - 0E07 */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c4, Thai_c3, Thai_c3, /* 0E08 - 0E0F */
	Thai_c4, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, /* 0E10 - 0E17 */
	Thai_c1, Thai_c1, Thai_c1, Thai_c2, Thai_c1, Thai_c2, Thai_c1, Thai_c2, /* 0E18 - 0E1F */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, /* 0E20 - 0E27 */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c2, Thai_c1, Thai_c1, Thai_c1, /* 0E28 - 0E2F */
	Thai_c1, Thai_da, Thai_c1, Thai_dr, Thai_da, Thai_da, Thai_da, Thai_da, /* 0E30 - 0E37 */
	Thai_db, Thai_db, Thai_db, Thai_xx, Thai_xx, Thai_xx, Thai_xx, Thai_c1, /* 0E38 - 0E3F */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_da, /* 0E40 - 0E47 */
	Thai_dt, Thai_dt, Thai_dt, Thai_dt, Thai_dt, Thai_da, Thai_da, Thai_c1, /* 0E48 - 0E4F */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_c1, /* 0E50 - 0E57 */
	Thai_c1, Thai_c1, Thai_c1, Thai_c1, Thai_xx, Thai_xx, Thai_xx, Thai_xx, /* 0E58 - 0E5F */
};

static ThaiCharClass
getThaiCharClass (unsigned short ch) {
	if (ch < 0x0E00 || ch > 0x0E5f)
		return Thai_CC_RESERVED;

	return ThaiCharClasses[ch - 0x0E00];
}

///////////////////////////////////////////////////////////////////////////////
/**
 * Function to check if uniCode is thai unicode or not
 */
bool isThai(unsigned short uniCode) {
	ThaiCharClass charClass = getThaiCharClass(uniCode);
	if (charClass == Thai_CC_RESERVED) {
		return false;
	}
#if THAIRESHAPERLOGS
	ALOGD("isThai, true");
#endif
	return true;
}

/**
 * Function to check whether unicode is a tone(zero width char) or not
 * returns true if unicode is not a tone(zero width char)
 **/
bool isThaiTone(unsigned short uniCode) {
	ThaiCharClass thaiClass = getThaiCharClass(uniCode);
	ThaiCharClass thaiFlag = (Thai_CF_ABOVE_VOWEL|Thai_CF_BELOW_VOWEL|Thai_CF_POST_VOWEL|Thai_CF_DEP_TONE);
#if THAIRESHAPERLOGS
	ALOGD("isThaiTone, unicode=0x%x, thaiClass=0x%x, thaiFlag=0x%x", uniCode, thaiClass, thaiFlag);
#endif
	if (thaiClass & thaiFlag) {
#if THAIRESHAPERLOGS
		ALOGD("isThaiTone, true");
#endif
		return true;
	}

#if THAIRESHAPERLOGS
	ALOGD("isThaiTone, false");
#endif
	return false;
}

/*bool isThai(unsigned short uniCode) {
	if ((uniCode >= KO_KAI) && (uniCode <= KHO_MUT)) {
#ifdef THAIRESHAPERLOGS
		LOGE("ThaiReshaper::isThai, true");
#endif
		return true;
	}
#ifdef THAIRESHAPERLOGS
	LOGE("ThaiReshaper::isThai, false");
#endif
	return false;
}

/*

 * Function to reshape thai character
 **/
///////////////////////////////////////////////////////////////////////////////
//[SISO][Saurabh] thai Reshaper - 15-07-11 starts
/**
 *  Function returns a positive integer or 0 if the c requires certain
 *  tones applied to it to be reshaped. whether tone is required to be reshaped is
 *  determined by using isThaiReshaperTone.
 **//**check***/
/*int isThaiReshaperCharhw(int32_t c) {

#ifdef THAIRESHAPERLOGS
	LOGE("ThaiReshaper::isThaiReshaperChar, unicode is %x", c);
#endif
	if ((c == YO_YING) || (c == THO_THAN)) {
		return THAIRESHAPEREMOVETAIL;
	} else if ((c == TO_PATAK) || (c == DO_CHADA)) {
		return THAIRESHAPEPULLDOWN;
	} else if ((c == PO_PLA) || (c == FO_FA) || (c == FO_FAN) ||(c == LO_CHULLA)) {
		return THAIRESHAPESHIFTLEFT;
	} else {
		return NOTTHAIRESHAPE;
	}



}

/**
 *  Function to check if reshaping is required on the tone.
 *  it returns integer value corresponding to the integer returned by
 *  isThaiReshaperChar, to check if current tone corresponds to the tones required
 *  to be reshaped for consonant in contention.
 **/
/*int isThaiReshaperTonehw(int32_t uniCode, int32_t consonantType) {
	if (uniCode == SARA_U || uniCode == SARA_UU || uniCode == PHINTHU) {
		if (consonantType == THAIRESHAPEREMOVETAIL)
			return THAIRESHAPEREMOVETAIL;
		else
			return THAIRESHAPEPULLDOWN;
	} else if (((uniCode >= MAI_EK ) && (uniCode <= NIKHAHIT)) || ((uniCode
			>= SARA_AM) && (uniCode <= SARA_Uee)) || (uniCode == MAI_HAN_AKAT)
			|| (uniCode == MAI_TAI_KHU)) {
		return THAIRESHAPESHIFTLEFT;
	} else {
		return NOTTHAIRESHAPE;
	}

}

/**
 *	Function returns a positive integer or 0 if the c requires certain
 *	tones applied to it to be reshaped. whether tone is required to be reshaped is
 *	determined by using isThaiReshaperTone.
 **/
int isThaiReshaperChar(unsigned short uniCode) {
	ThaiCharClass thaiClass = getThaiCharClass(uniCode);

#if THAIRESHAPERLOGS
	ALOGD("isThaiReshaperChar, unicode is 0x%x, thaiClass is 0x%x", uniCode, thaiClass);
#endif
	if (thaiClass & Thai_CF_REMOVE_TAIL) {
		return THAI_REMOVE_TAIL;
	} else if (thaiClass & Thai_CF_SHIFT_DOWN) {
		return THAI_PULLDOWN_TAIL;
	} else if (thaiClass & Thai_CF_SHIFT_LEFT) {
		return THAI_SHIFT_LEFT;
	} else if (thaiClass == Thai_dt) {
		return THAI_MOVE_TONE;
	} else {
		return THAI_NO_RESHAPE;
	}
}

/**
 *  Function to check if reshaping is required on the tone.
 *  it returns integer value corresponding to the integer returned by
 *  isThaiReshaperChar, to check if current tone corresponds to the tones required
 *  to be reshaped for consonant in contention.
 **/
int isThaiReshaperTone(unsigned short uniCode, int consonantType) {
#if DEBUG_THAI_RESHAPER
    ALOGD("isThaiReshaperTone, unicode is 0x%x, consonantType is 0x%x", uniCode, consonantType);
#endif
    ThaiCharClass thaiClass = getThaiCharClass(uniCode);

    switch (consonantType) {
    case THAI_REMOVE_TAIL: /* Fall through */
    case THAI_PULLDOWN_TAIL:
        if (thaiClass & Thai_CF_BELOW_VOWEL) {
            return consonantType;
        }

	// SAMSUNG PLM P140314-01891 FIX_Start (unni.krish@samsung.com)
	if (thaiClass & Thai_CF_DEP_TONE) {
		return THAI_MOVE_TONE;
	}
	// SAMSUNG PLM P140314-01891 FIX_end

        break;
    case THAI_SHIFT_LEFT:
        if (thaiClass & (Thai_CF_ABOVE_VOWEL|Thai_CF_POST_VOWEL|Thai_CF_DEP_TONE)) {
            return consonantType;
        }
        break;
    case THAI_MOVE_TONE:
        if (thaiClass & Thai_CF_POST_VOWEL) {
            return THAI_COMBINE_SARAAM;
        }
        if (thaiClass & (Thai_CF_CONSONANT|Thai_CF_BELOW_VOWEL)) {
            return THAI_PULLDOWN_TONE;
        }
        break;
    default:
        break;
    }
    return THAI_NO_RESHAPE;
}


/**
 * Function to reshape tone in composing state
 */
int reshapeTone(unsigned short** toneptr, int consonanttype) {
	int toneLen = 1;
	unsigned short *tone = NULL;
	tone = *toneptr;
	switch (consonanttype) {

	case THAI_PULLDOWN_TAIL:
		if (isThaiReshaperTone(*tone, consonanttype) == THAI_PULLDOWN_TAIL) {
			if (*tone == SARA_U) {
				*tone = SARA_U_DOWN; /* SARA_U_PULL_DOWN */
			} else if (*tone == SARA_UU ) {
				*tone = SARA_UU_DOWN; /* SARA_UU_PULL_DOWN */
			} else {
				*tone = PHINTHU_DOWN; /*PHINTHU_DOWN*/
			}
		}
		break;

	case THAI_SHIFT_LEFT:
		if (isThaiReshaperTone(*tone, consonanttype) == THAI_SHIFT_LEFT) {
			switch (*tone) {
			case MAI_HAN_AKAT:
				*tone = MAI_HAN_AKAT_LEFT_SHIFT; /* MAI_HAN_AKAT_LEFT_SHIFT */
				break;
			case SARA_I:
				*tone = SARA_I_LEFT_SHIFT; /* SARA_I_LEFT_SHIFT */
				break;
			case SARA_Ii:
				*tone = SARA_Ii_LEFT_SHIFT; /* SARA_Ii_LEFT_SHIFT */
				break;
			case SARA_Ue:
				*tone = SARA_Ue_LEFT_SHIFT; /* SARA_Ue_LEFT_SHIFT */
				break;
			case SARA_Uee:
				*tone = SARA_Uee_LEFT_SHIFT; /* SARA_Uee_LEFT_SHIFT */
				break;
			case MAI_TAI_KHU:
				*tone = MAI_TAI_KHU_LEFT_SHIFT; /* MAI_TAI_KHU_LEFT_SHIFT */
				break;
			case MAI_EK:
				*tone = MAI_EK_LEFT_SHIFT; /* MAI_EK_LEFT_SHIFT */
				break;
			case MAI_THO:
				*tone = MAI_THO_LEFT_SHIFT; /* MAI_THO_LEFT_SHIFT */
				break;
			case MAI_TRI:
				*tone = MAI_TRI_LEFT_SHIFT; /*MAI_TRI_LEFT_SHIFT*/
				break;
			case MAI_CHATTAWA:
				*tone = MAI_CHATTAWA_LEFT_SHIFT; /*MAI_CHATTAWA_LEFT_SHIFT*/
				break;
			case THANTHAKHAT:
				*tone = THANTHAKHAT_LEFT_SHIFT; /*THANTHAKHAT_LEFT_SHIFT*/
				break;
			case NIKHAHIT:
				*tone = NIKHAHIT_LEFT_SHIFT; /*NIKHAHIT_LEFT_SHIFT*/
				break;
			default:
				*tone = SARA_AM_LEFT_SHIFT; /*SARA_AM_LEFT_SHIFT*/
				break;
			}
		}
		break;

	default:
		ALOGE("reshapeTone, consonanttype = 0x%x!!", consonanttype);
		break;
	}
#if THAIRESHAPERLOGS
	ALOGD("reshapeTone, size of out buffer is %d", toneLen);
#endif
	return toneLen;
}

/**
 * Function to check whether Reshaping is required or not.
 **/
bool hasThai(unsigned short uniCode[], size_t len) {
	int consonanttype;
	for (int i = 0; i < len; i++) {
		consonanttype = isThaiReshaperChar(uniCode[i]);

		if (consonanttype != THAI_NO_RESHAPE) {

			if (consonanttype == THAI_MOVE_TONE) {
				if( i > 0 && isThaiReshaperTone(uniCode[i-1], consonanttype) != THAI_NO_RESHAPE ) {
					return true;
				}
				continue;
			}
			for (int j = i + 1; ((j <= (i + 3)) && (j < len)); j++) {

				if (!isThaiTone(uniCode[j])) {
					if (consonanttype != THAI_SHIFT_LEFT) {
#if THAIRESHAPERLOGS
						ALOGD("hasThai, break1");
#endif
						break;
					} else {
						if (uniCode[j] != SARA_AM) {
#if THAIRESHAPERLOGS
							ALOGD("hasThai, break2");
#endif
							break;
						}
					}
				}

				if (isThaiReshaperTone(uniCode[j], consonanttype) != THAI_NO_RESHAPE) {
#if THAIRESHAPERLOGS
					ALOGD("hasThai, true..");
#endif
					return true;
				}
			}
		}
	}

	return false;
}


/**
 * [SNMC Brower Team] [yogendra Singh] 
 *  Function to replace Thai unicode characters in place in an input string if reshaping is required.
 *  It returns true if atleast once character is replaced.
 **/
/*bool thaiReshaperInPlace(UChar** originalString, size_t stringLength){
    UChar* string = *originalString;
    int result = NOTTHAIRESHAPE;
    bool replaced = false;	
    for(size_t i=0; i<stringLength; i++)
    {
        if(isThai(string[i])){
            int cT=isThaiReshaperCharhw(string[i]);
            switch(cT){
                case THAIRESHAPEREMOVETAIL: 
                    result=isThaiReshaperTonehw(string[++i],cT);
                    if(equal(result,THAIRESHAPEREMOVETAIL)){
                        if(equal(string[i-1], YO_YING)) {
                            string[i-1]= YO_YING_CUT_TAIL;
                        }
                        else {
                            string[i-1]= THO_THAN_CUT_TAIL;
                        }
                        replaced = true;
                    }
                    else if(equal(result,NOTTHAIRESHAPE)) {
                        i--;
                    }

                    break;
                case THAIRESHAPEPULLDOWN:
                    result=isThaiReshaperTonehw(string[++i],cT);
                    if(equal(result, THAIRESHAPEPULLDOWN)){
                        if (equal(string[i], SARA_U)) {
                            string[i] = SARA_U_DOWN; // SARA_U_PULL_DOWN 
                        } else if (equal(string[i], SARA_UU)) {
                            string[i] = SARA_UU_DOWN; // SARA_UU_PULL_DOWN 
                        } else {
                            string[i] = PHINTHU_DOWN; //PHINTHU_DOWN
                        }
                        replaced = true;
                    }
                    else if(equal(result,NOTTHAIRESHAPE)){
                        i--;
                    }
                    break;
                case THAIRESHAPESHIFTLEFT: 
                    { 
                            bool isThirdCharacterIsThaiTone=!isNotThaiTone(string[i+2]);
                            result=isThaiReshaperTonehw(string[++i],cT);
                            if (equal(result, THAIRESHAPESHIFTLEFT)) {
                                    if (equal(string[i] , MAI_HAN_AKAT)) {
                                            string[i] = MAI_HAN_AKAT_LEFT_SHIFT; // MAI_HAN_AKAT_LEFT_SHIFT 
                                    } else if (equal(string[i] , SARA_I)) {
                                            string[i] = SARA_I_LEFT_SHIFT; // SARA_I_LEFT_SHIFT 
                                    }else if (equal(string[i] , SARA_AM)) {
                                            string[i] = SARA_AM_LEFT_SHIFT; // SARA_AM_LEFT_SHIFT 
                                    } else if (equal(string[i] ,SARA_Ii)) {
                                            string[i] = SARA_Ii_LEFT_SHIFT; // SARA_Ii_LEFT_SHIFT 
                                    } else if (equal(string[i] ,SARA_Ue)) {
                                            string[i] = SARA_Ue_LEFT_SHIFT; // SARA_Ue_LEFT_SHIFT 
                                    } else if (equal(string[i] , SARA_Uee)) {
                                            string[i] = SARA_Uee_LEFT_SHIFT; // SARA_Uee_LEFT_SHIFT 
                                    } else if (equal(string[i] , MAI_TAI_KHU)) {
                                            string[i] = MAI_TAI_KHU_LEFT_SHIFT; // MAI_TAI_KHU_LEFT_SHIFT /
                                    } else if (equal(string[i] , MAI_EK)) {
                                            string[i] = MAI_EK_LEFT_SHIFT; // MAI_EK_LEFT_SHIFT 
                                    } else if (equal(string[i] , MAI_THO)) {
                                            string[i] = MAI_THO_LEFT_SHIFT; // MAI_THO_LEFT_SHIFT 
                                    } else if (equal(string[i] , MAI_TRI)) {
                                            string[i] = MAI_TRI_LEFT_SHIFT; //MAI_TRI_LEFT_SHIFT
                                    } else if (equal(string[i] , MAI_CHATTAWA)) {
                                            string[i] = MAI_CHATTAWA_LEFT_SHIFT; //MAI_CHATTAWA_LEFT_SHIFT
                                    } else if (equal(string[i] , THANTHAKHAT)) {
                                            string[i] = THANTHAKHAT_LEFT_SHIFT; //THANTHAKHAT_LEFT_SHIFT
                                    } else if (equal(string[i] , NIKHAHIT)) {
                                            string[i] = NIKHAHIT_LEFT_SHIFT; //NIKHAHIT_LEFT_SHIFT
                                    } 
                                    replaced = true;
                            }
                            else if(equal(result,NOTTHAIRESHAPE)){
                                    i--;
                            }
                            if(isThirdCharacterIsThaiTone==true){
                                    result=isThaiReshaperTonehw(string[++i],cT);
                                    if (equal(result, THAIRESHAPESHIFTLEFT)) {
                                            if (equal(string[i] , MAI_HAN_AKAT)) {
                                                    string[i] = MAI_HAN_AKAT_LEFT_SHIFT; // MAI_HAN_AKAT_LEFT_SHIFT 
                                            }else if (equal(string[i] , SARA_I)) {
                                                    string[i] = SARA_I_LEFT_SHIFT; // SARA_I_LEFT_SHIFT 
                                            }else if (equal(string[i] , SARA_AM)) {
                                                    string[i] = SARA_AM_LEFT_SHIFT; // SARA_AM_LEFT_SHIFT
                                            } else if (equal(string[i] ,SARA_Ii)) {
                                                    string[i] = SARA_Ii_LEFT_SHIFT; // SARA_Ii_LEFT_SHIFT 
                                            } else if (equal(string[i] ,SARA_Ue)) {
                                                    string[i] = SARA_Ue_LEFT_SHIFT; // SARA_Ue_LEFT_SHIFT 
                                            } else if (equal(string[i] , SARA_Uee)) {
                                                    string[i] = SARA_Uee_LEFT_SHIFT; // SARA_Uee_LEFT_SHIFT 
                                            } else if (equal(string[i] , MAI_TAI_KHU)) {
                                                    string[i] = MAI_TAI_KHU_LEFT_SHIFT; // MAI_TAI_KHU_LEFT_SHIFT /
                                            } else if (equal(string[i] , MAI_EK)) {
                                                    string[i] = MAI_EK_LEFT_SHIFT; // MAI_EK_LEFT_SHIFT 
                                            } else if (equal(string[i] , MAI_THO)) {
                                                    string[i] = MAI_THO_LEFT_SHIFT; // MAI_THO_LEFT_SHIFT 
                                            } else if (equal(string[i] , MAI_TRI)) {
                                                    string[i] = MAI_TRI_LEFT_SHIFT; //MAI_TRI_LEFT_SHIFT
                                            } else if (equal(string[i] , MAI_CHATTAWA)) {
                                                    string[i] = MAI_CHATTAWA_LEFT_SHIFT; //MAI_CHATTAWA_LEFT_SHIFT
                                            } else if (equal(string[i] , THANTHAKHAT)) {
                                                    string[i] = THANTHAKHAT_LEFT_SHIFT; //THANTHAKHAT_LEFT_SHIFT
                                            } else if (equal(string[i] , NIKHAHIT)) {
                                                    string[i] = NIKHAHIT_LEFT_SHIFT; //NIKHAHIT_LEFT_SHIFT
                                            } 
                                            replaced = true;
                                    }
                                    else if(equal(result,NOTTHAIRESHAPE)){
                                            i--;
                                    }
                            }
                    }
                    break;
                case MAXBUFFERLENGTH: 
                    break;
            }
        }
        }
return replaced;
}*/


/**
 * Function to reshape thai character
 **/
size_t thaiReshaper(unsigned short in[], size_t inLen, unsigned short **outptr) {
	size_t outLen = 0;
	size_t totaloutLen = inLen;
	//init
	unsigned short *out = NULL;
	out = *outptr;
	int consonanttype = THAI_NO_RESHAPE;
	for (int i = 0; i < inLen; i++) {
		consonanttype = isThaiReshaperChar(in[i]);
		out[outLen] = in[i];
		outLen++;

		switch (consonanttype) {

		case THAI_REMOVE_TAIL:
			for (int j = i + 1; ((j <= (i + 3)) && (j < inLen)); j++) {
				if (in[j] == SARA_AM) {
					for (int k = i + 1; k < j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j - 1;
					break;
				}
				if (!isThaiTone(in[j])) {
					for (int k = i + 1; k < j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j - 1;
					break;
				}
				consonanttype = isThaiReshaperTone(in[j], consonanttype);
				if (consonanttype == THAI_REMOVE_TAIL) {
					if (in[i] == YO_YING) {
						out[outLen - 1] = YO_YING_CUT_TAIL; /* without lower part */
					} else {
						out[outLen - 1] = THO_THAN_CUT_TAIL; /*  without lower part */
					}
					for (int k = i + 1; k <= j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j;
					break;
				}
                
				// SAMSUNG PLM P140314-01891 FIX_Start
                checkAndBreak(consonanttype, THAI_MOVE_TONE);
				// SAMSUNG PLM P140314-01891 FIX_End
			}
			break;

		case THAI_PULLDOWN_TAIL:
			for (int j = i + 1; ((j <= (i + 3)) && (j < inLen)); j++) {
				if (in[j] == SARA_AM) {
					for (int k = i + 1; k < j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j - 1;
					break;
				}
				if (!isThaiTone(in[j])) {
					for (int k = i + 1; k < j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j - 1;
					break;
				}

				consonanttype = isThaiReshaperTone(in[j], consonanttype);
				if (consonanttype == THAI_PULLDOWN_TAIL) {
					if (in[j] == SARA_U) {
						out[outLen] = SARA_U_DOWN; /* SARA_U_PULL_DOWN */
					} else if (in[j] == SARA_UU) {
						out[outLen] = SARA_UU_DOWN; /* SARA_UU_PULL_DOWN */
					} else {
						out[outLen] = PHINTHU_DOWN; /*PHINTHU_DOWN*/
					}
					outLen++;
					for (int k = i + 1; k < j; k++) {
						out[outLen] = in[k];
						outLen++;
					}
					i = j;
					break;

				}
                
				// SAMSUNG PLM P140314-01891 FIX_Start
                checkAndBreak(consonanttype, THAI_MOVE_TONE);
				// SAMSUNG PLM P140314-01891 FIX_End
			}
			break;

		case THAI_SHIFT_LEFT:
			int j;
			for (j = i + 1; ((j <= (i + 3)) && (j < inLen)); j++) {
				if (!isThaiTone(in[j])) {
					break;
				}
				if (isThaiReshaperTone(in[j], consonanttype) == THAI_SHIFT_LEFT) {

					switch (in[j]) {
					case MAI_HAN_AKAT:
						out[outLen++] = MAI_HAN_AKAT_LEFT_SHIFT; /* MAI_HAN_AKAT_LEFT_SHIFT */
						break;
					case SARA_I:
						out[outLen++] = SARA_I_LEFT_SHIFT; /* SARA_I_LEFT_SHIFT */
						break;
					case SARA_Ii:
						out[outLen++] = SARA_Ii_LEFT_SHIFT; /* SARA_Ii_LEFT_SHIFT */
						break;
					case SARA_Ue:
						out[outLen++] = SARA_Ue_LEFT_SHIFT; /* SARA_Ue_LEFT_SHIFT */
						break;
					case SARA_Uee:
						out[outLen++] = SARA_Uee_LEFT_SHIFT; /* SARA_Uee_LEFT_SHIFT */
						break;
					case MAI_TAI_KHU:
						out[outLen++] = MAI_TAI_KHU_LEFT_SHIFT; /* MAI_TAI_KHU_LEFT_SHIFT */
						break;
					case MAI_EK:
						out[outLen++] = MAI_EK_LEFT_SHIFT; /* MAI_EK_LEFT_SHIFT */
						break;
					case MAI_THO:
						out[outLen++] = MAI_THO_LEFT_SHIFT; /* MAI_THO_LEFT_SHIFT */
						break;
					case MAI_TRI:
						out[outLen++] = MAI_TRI_LEFT_SHIFT; /*MAI_TRI_LEFT_SHIFT*/
						break;
					case MAI_CHATTAWA:
						out[outLen++] = MAI_CHATTAWA_LEFT_SHIFT; /*MAI_CHATTAWA_LEFT_SHIFT*/
						break; 
					case THANTHAKHAT:
						out[outLen++] = THANTHAKHAT_LEFT_SHIFT; /*THANTHAKHAT_LEFT_SHIFT*/
						break;
					case NIKHAHIT:
						out[outLen++] = NIKHAHIT_LEFT_SHIFT; /*NIKHAHIT_LEFT_SHIFT*/
						break;
					default:
						out[outLen++] = SARA_AM_LEFT_SHIFT; /*SARA_AM_LEFT_SHIFT*/
						break;
					}
				} else {
					out[outLen++] = in[j];
				}
			}
			/* i is assigned j-1 as j has value i+4 ,
			 and since i will get incremeneted again it will become i+5,
			 to avaid it j is decremented once and then assigned to i
			*/
			i = j - 1;
			break;

		case THAI_MOVE_TONE:
			if ((i+1 < inLen) && isThaiReshaperTone(in[i+1],consonanttype) == THAI_COMBINE_SARAAM) {
				switch (in[i]) {
				case MAI_EK:
					out[outLen-1] = MAI_EK_SARA_AM;
					out[outLen++] = SARA_AA;
					break;
				case MAI_THO:
					out[outLen-1] = MAI_THO_SARA_AM;
					out[outLen++] = SARA_AA;
					break;
				case MAI_TRI:
					out[outLen-1] = MAI_TRI_SARA_AM;
					out[outLen++] = SARA_AA;
					break;
				case MAI_CHATTAWA:
					out[outLen-1] = MAI_CHATTAWA_SARA_AM;
					out[outLen++] = SARA_AA;
					break;
				default:
					out[outLen++] = SARA_AM;
					break;
				}
				i++;
			} else if (i > 0 && isThaiReshaperTone(in[i-1],consonanttype) == THAI_PULLDOWN_TONE) {
				switch (in[i]) {
				case MAI_EK:
					out[outLen-1] = MAI_EK_DOWN;
					break;
				case MAI_THO:
					out[outLen-1] = MAI_THO_DOWN;
					break;
				case MAI_TRI:
					out[outLen-1] = MAI_TRI_DOWN;
					break;
				case MAI_CHATTAWA:
					out[outLen-1] = MAI_CHATTAWA_DOWN;
					break;
				case THANTHAKHAT:
					out[outLen-1] = THANTHAKHAT_DOWN;
					break;
				default:
					ALOGE("thaiReshaper, THAI_PULLDOWN_TONE error!!");
					break;
				}
			}
			break;

		default:
			break;
		}
	}
#if THAIRESHAPERLOGS
	for (int l = 0;l < outLen;l++)
		ALOGD("thaiReshaper,  out buffer[%d] is %x", l, out[l]);
	ALOGD("thaiReshaper, length after reshaping is %d", outLen);
#endif
	return outLen;
}

/**
 * Function to thai+nospace+line-feed showing up a junk char fix
 */
void thaiLF2Space(unsigned short utf[], size_t len) {
	for (int i = 1; i < len; i++) {
		if (utf[i] == 10 && isThai(utf[i-1])) {
			utf[i] = 32;
		}
	}
}

/*bool isNotThaiTone(int unicode) {

    if (((unicode >= KO_KAI) && (unicode <= SARA_A)) 
        || ((unicode >= FONG_MAN) && (unicode <= KHO_MUT))
        || ((unicode >= SYMBOL_BAHT) && (unicode <= MAIYA_MOK))
        || (unicode == SARA_AA) ) {
#ifdef DEBUG_THAI_RESHAPER
        ALOGD("isNotThaiTone, true");
#endif
        return true;
    }
    return false;
}*/
//SAMSUNG CHANGE <<
