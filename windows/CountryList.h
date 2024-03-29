#ifndef COUNTRY_LIST_H
#define COUNTRY_LIST_H

#include <string>
#include <stdint.h>

enum
{
	COUNTRY_NONE = -1,
	COUNTRY_ANDORRA,
	COUNTRY_UNITED_ARAB_EMIRATES,
	COUNTRY_AFGHANISTAN,
	COUNTRY_ANTIGUA_AND_BARBUDA,
	COUNTRY_ANGUILLA,
	COUNTRY_ALBANIA,
	COUNTRY_ARMENIA,
	COUNTRY_NETHERLANDS_ANTILLES,
	COUNTRY_ANGOLA,
	COUNTRY_ANTARCTICA,
	COUNTRY_ARGENTINA,
	COUNTRY_AMERICAN_SAMOA,
	COUNTRY_AUSTRIA,
	COUNTRY_AUSTRALIA,
	COUNTRY_ARUBA,
	COUNTRY_ALAND,
	COUNTRY_AZERBAIJAN,
	COUNTRY_BOSNIA_AND_HERZEGOVINA,
	COUNTRY_BARBADOS,
	COUNTRY_BANGLADESH,
	COUNTRY_BELGIUM,
	COUNTRY_BURKINA_FASO,
	COUNTRY_BULGARIA,
	COUNTRY_BAHRAIN,
	COUNTRY_BURUNDI,
	COUNTRY_BENIN,
	COUNTRY_BERMUDA,
	COUNTRY_BRUNEI_DARUSSALAM,
	COUNTRY_BOLIVIA,
	COUNTRY_BRAZIL,
	COUNTRY_BAHAMAS,
	COUNTRY_BHUTAN,
	COUNTRY_BOUVET_ISLAND,
	COUNTRY_BOTSWANA,
	COUNTRY_BELARUS,
	COUNTRY_BELIZE,
	COUNTRY_CANADA,
	COUNTRY_COCOS_ISLANDS,
	COUNTRY_THE_DEMOCRATIC_REPUBLIC_OF_THE_CONGO,
	COUNTRY_CENTRAL_AFRICAN_REPUBLIC,
	COUNTRY_CONGO,
	COUNTRY_SWITZERLAND,
	COUNTRY_COTE_DIVOIRE,
	COUNTRY_COOK_ISLANDS,
	COUNTRY_CHILE,
	COUNTRY_CAMEROON,
	COUNTRY_CHINA,
	COUNTRY_COLOMBIA,
	COUNTRY_COSTA_RICA,
	COUNTRY_SERBIA_AND_MONTENEGRO,
	COUNTRY_CUBA,
	COUNTRY_CAPE_VERDE,
	COUNTRY_CHRISTMAS_ISLAND,
	COUNTRY_CYPRUS,
	COUNTRY_CZECH_REPUBLIC,
	COUNTRY_GERMANY,
	COUNTRY_DJIBOUTI,
	COUNTRY_DENMARK,
	COUNTRY_DOMINICA,
	COUNTRY_DOMINICAN_REPUBLIC,
	COUNTRY_ALGERIA,
	COUNTRY_ECUADOR,
	COUNTRY_ESTONIA,
	COUNTRY_EGYPT,
	COUNTRY_WESTERN_SAHARA,
	COUNTRY_ERITREA,
	COUNTRY_SPAIN,
	COUNTRY_ETHIOPIA,
	COUNTRY_EUROPE,
	COUNTRY_FINLAND,
	COUNTRY_FIJI,
	COUNTRY_FALKLAND_ISLANDS,
	COUNTRY_MICRONESIA,
	COUNTRY_FAROE_ISLANDS,
	COUNTRY_FRANCE,
	COUNTRY_GABON,
	COUNTRY_UNITED_KINGDOM,
	COUNTRY_GRENADA,
	COUNTRY_GEORGIA,
	COUNTRY_FRENCH_GUIANA,
	COUNTRY_GUERNSEY,
	COUNTRY_GHANA,
	COUNTRY_GIBRALTAR,
	COUNTRY_GREENLAND,
	COUNTRY_GAMBIA,
	COUNTRY_GUINEA,
	COUNTRY_GUADELOUPE,
	COUNTRY_EQUATORIAL_GUINEA,
	COUNTRY_GREECE,
	COUNTRY_SOUTH_GEORGIA_AND_THE_SOUTH_SANDWICH_ISLANDS,
	COUNTRY_GUATEMALA,
	COUNTRY_GUAM,
	COUNTRY_GUINEA_BISSAU,
	COUNTRY_GUYANA,
	COUNTRY_HONG_KONG,
	COUNTRY_HEARD_ISLAND_AND_MCDONALD_ISLANDS,
	COUNTRY_HONDURAS,
	COUNTRY_CROATIA,
	COUNTRY_HAITI,
	COUNTRY_HUNGARY,
	COUNTRY_INDONESIA,
	COUNTRY_IRELAND,
	COUNTRY_ISRAEL,
	COUNTRY_ISLE_OF_MAN,
	COUNTRY_INDIA,
	COUNTRY_BRITISH_INDIAN_OCEAN_TERRITORY,
	COUNTRY_IRAQ,
	COUNTRY_IRAN,
	COUNTRY_ICELAND,
	COUNTRY_ITALY,
	COUNTRY_JERSEY,
	COUNTRY_JAMAICA,
	COUNTRY_JORDAN,
	COUNTRY_JAPAN,
	COUNTRY_KENYA,
	COUNTRY_KYRGYZSTAN,
	COUNTRY_CAMBODIA,
	COUNTRY_KIRIBATI,
	COUNTRY_COMOROS,
	COUNTRY_SAINT_KITTS_AND_NEVIS,
	COUNTRY_DEMOCRATIC_PEOPLES_REPUBLIC_OF_KOREA,
	COUNTRY_SOUTH_KOREA,
	COUNTRY_KUWAIT,
	COUNTRY_CAYMAN_ISLANDS,
	COUNTRY_KAZAKHSTAN,
	COUNTRY_LAO_PEOPLES_DEMOCRATIC_REPUBLIC,
	COUNTRY_LEBANON,
	COUNTRY_SAINT_LUCIA,
	COUNTRY_LIECHTENSTEIN,
	COUNTRY_SRI_LANKA,
	COUNTRY_LIBERIA,
	COUNTRY_LESOTHO,
	COUNTRY_LITHUANIA,
	COUNTRY_LUXEMBOURG,
	COUNTRY_LATVIA,
	COUNTRY_LIBYAN_ARAB_JAMAHIRIYA,
	COUNTRY_MOROCCO,
	COUNTRY_MONACO,
	COUNTRY_MOLDOVA,
	COUNTRY_MONTENEGRO,
	COUNTRY_MADAGASCAR,
	COUNTRY_MARSHALL_ISLANDS,
	COUNTRY_MACEDONIA,
	COUNTRY_MALI,
	COUNTRY_MYANMAR,
	COUNTRY_MONGOLIA,
	COUNTRY_MACAO,
	COUNTRY_NORTHERN_MARIANA_ISLANDS,
	COUNTRY_MARTINIQUE,
	COUNTRY_MAURITANIA,
	COUNTRY_MONTSERRAT,
	COUNTRY_MALTA,
	COUNTRY_MAURITIUS,
	COUNTRY_MALDIVES,
	COUNTRY_MALAWI,
	COUNTRY_MEXICO,
	COUNTRY_MALAYSIA,
	COUNTRY_MOZAMBIQUE,
	COUNTRY_NAMIBIA,
	COUNTRY_NEW_CALEDONIA,
	COUNTRY_NIGER,
	COUNTRY_NORFOLK_ISLAND,
	COUNTRY_NIGERIA,
	COUNTRY_NICARAGUA,
	COUNTRY_NETHERLANDS,
	COUNTRY_NORWAY,
	COUNTRY_NEPAL,
	COUNTRY_NAURU,
	COUNTRY_NIUE,
	COUNTRY_NEW_ZEALAND,
	COUNTRY_OMAN,
	COUNTRY_PANAMA,
	COUNTRY_PERU,
	COUNTRY_FRENCH_POLYNESIA,
	COUNTRY_PAPUA_NEW_GUINEA,
	COUNTRY_PHILIPPINES,
	COUNTRY_PAKISTAN,
	COUNTRY_POLAND,
	COUNTRY_SAINT_PIERRE_AND_MIQUELON,
	COUNTRY_PITCAIRN,
	COUNTRY_PUERTO_RICO,
	COUNTRY_PALESTINIAN_TERRITORY,
	COUNTRY_PORTUGAL,
	COUNTRY_PALAU,
	COUNTRY_PARAGUAY,
	COUNTRY_QATAR,
	COUNTRY_REUNION,
	COUNTRY_ROMANIA,
	COUNTRY_SERBIA,
	COUNTRY_RUSSIAN_FEDERATION,
	COUNTRY_RWANDA,
	COUNTRY_SAUDI_ARABIA,
	COUNTRY_SOLOMON_ISLANDS,
	COUNTRY_SEYCHELLES,
	COUNTRY_SUDAN,
	COUNTRY_SWEDEN,
	COUNTRY_SINGAPORE,
	COUNTRY_SAINT_HELENA,
	COUNTRY_SLOVENIA,
	COUNTRY_SVALBARD_AND_JAN_MAYEN,
	COUNTRY_SLOVAKIA,
	COUNTRY_SIERRA_LEONE,
	COUNTRY_SAN_MARINO,
	COUNTRY_SENEGAL,
	COUNTRY_SOMALIA,
	COUNTRY_SURINAME,
	COUNTRY_SAO_TOME_AND_PRINCIPE,
	COUNTRY_EL_SALVADOR,
	COUNTRY_SYRIAN_ARAB_REPUBLIC,
	COUNTRY_SWAZILAND,
	COUNTRY_TURKS_AND_CAICOS_ISLANDS,
	COUNTRY_CHAD,
	COUNTRY_FRENCH_SOUTHERN_TERRITORIES,
	COUNTRY_TOGO,
	COUNTRY_THAILAND,
	COUNTRY_TAJIKISTAN,
	COUNTRY_TOKELAU,
	COUNTRY_TIMOR_LESTE,
	COUNTRY_TURKMENISTAN,
	COUNTRY_TUNISIA,
	COUNTRY_TONGA,
	COUNTRY_TURKEY,
	COUNTRY_TRINIDAD_AND_TOBAGO,
	COUNTRY_TUVALU,
	COUNTRY_TAIWAN,
	COUNTRY_TANZANIA,
	COUNTRY_UKRAINE,
	COUNTRY_UGANDA,
	COUNTRY_UNITED_STATES_MINOR_OUTLYING_ISLANDS,
	COUNTRY_UNITED_STATES,
	COUNTRY_URUGUAY,
	COUNTRY_UZBEKISTAN,
	COUNTRY_VATICAN,
	COUNTRY_SAINT_VINCENT_AND_THE_GRENADINES,
	COUNTRY_VENEZUELA,
	COUNTRY_BRITISH_VIRGIN_ISLANDS,
	COUNTRY_US_VIRGIN_ISLANDS,
	COUNTRY_VIET_NAM,
	COUNTRY_VANUATU,
	COUNTRY_WALLIS_AND_FUTUNA,
	COUNTRY_SAMOA,
	COUNTRY_YEMEN,
	COUNTRY_MAYOTTE,
	COUNTRY_YUGOSLAVIA,
	COUNTRY_SOUTH_AFRICA,
	COUNTRY_ZAMBIA,
	COUNTRY_ZIMBABWE,
	COUNTRY_RUSSIA,
	COUNTRY_EUROPEAN_UNION,
};

int getCountryByName(const std::string& name);
uint16_t getCountryCode(int index);

#endif /* COUNTRY_LIST_H */
