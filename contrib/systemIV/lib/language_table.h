#ifndef LANGUAGE_TABLE_H
#define LANGUAGE_TABLE_H

#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"


#define    PREFS_INTERFACE_LANGUAGE        "InterfaceTextLanguage"


class Language;


class LanguageTable
{
protected:
	BTree       <char *> m_baseLanguage;                                                // Reference language (english)
    BTree       <char *> m_translation;                                                 // replacement translations
    BTree       <char *> m_translationAdditional;                                       // replacement additional translations

	char        *m_pathAdditionalTranslation;

	char        *m_defaultLanguage;
	bool        m_onlyDefaultLanguageSelectable;

    void        LoadBaseLanguage    (const char *_filename);
    void        LoadTranslation     (const char *_filename);

	void        LoadLanguage        (const char *_filename, BTree<char *> &_langTable, bool _facultative = false, const char *_separators = NULL);

    bool        LoadLanguageCaption (Language *lang);

#ifdef TRACK_LANGUAGEPHRASE_ERRORS
    BTree       <char *> m_languagePhraseErrors;
    void        ClearLanguagePhraseErrors             ();

    bool        LanguageTableIsNewLanguagePhraseError (const char *_file, int _line, char flag, char *subject, int nbFound);
#endif

public:
	Language    *m_lang;
    LList       <Language *> m_languages;

public:
	LanguageTable   ();
	~LanguageTable  ();

    void        Initialise              ();        
    void        SetAdditionalTranslation(const char *_pathAdditionalTranslation);
    void        LoadLanguages           ();

	void        SetDefaultLanguage      (const char *_name, bool _onlySelectable = false);
	void        LoadCurrentLanguage     (const char *_name = NULL);
	bool        SaveCurrentLanguage     (Language *_lang);
	bool        GetLanguageSelectable   (const char *_name);
	void        SetLanguageSelectable   (const char *_name, bool _selectable);

    void        TestTranslation         (const char *_logFilename);

    void        ClearBaseLanguage       ();
    void        ClearTranslation        ();

    bool        DoesPhraseExist         (const char *_key);
    bool        DoesTranslationExist    (const char *_key);

	const char        *LookupBasePhrase       (const char *_key);
	const char        *LookupPhrase           (const char *_key);
	const char        *LookupPhraseAdditional (const char *_key);

    int         ReplaceStringFlag       (char flag, const char *string, char *subject);
    int         ReplaceIntegerFlag      (char flag, int num, char *subject);

    bool        DoesFlagExist           (char flag, const char *subject);

#ifdef TRACK_LANGUAGEPHRASE_ERRORS
    const char *DebugLookupPhrase      (const char *_file, int _line, const char *_key);
    int         DebugReplaceStringFlag  (const char *_file, int _line, char flag, const char *string, char *subject);
    int         DebugReplaceIntegerFlag (const char *_file, int _line, char flag, int num, char *subject);
#endif

};



extern LanguageTable *g_languageTable;

#ifdef TRACK_LANGUAGEPHRASE_ERRORS
	#define LANGUAGEPHRASE(x)                           g_languageTable->DebugLookupPhrase(__FILE__,__LINE__,x)
	#define LPREPLACESTRINGFLAG(flag,string,subject)    g_languageTable->DebugReplaceStringFlag(__FILE__,__LINE__,flag,string,subject)
	#define LPREPLACEINTEGERFLAG(flag,num,subject)      g_languageTable->DebugReplaceIntegerFlag(__FILE__,__LINE__,flag,num,subject)
#else
	#define LANGUAGEPHRASE(x)                           g_languageTable->LookupPhrase(x)
	#define LPREPLACESTRINGFLAG(flag,string,subject)    g_languageTable->ReplaceStringFlag(flag,string,subject)
	#define LPREPLACEINTEGERFLAG(flag,num,subject)      g_languageTable->ReplaceIntegerFlag(flag,num,subject)
#endif

#define LANGUAGEPHRASEADDITIONAL(x)                     g_languageTable->LookupPhraseAdditional(x)



// ============================================================================


class Language
{
public:
    char    m_name      [256];
    char    m_path      [256];
    char    m_caption   [256];
	bool    m_selectable;

    char    m_pathAdditional[256];

public:
    Language();
};


#endif
