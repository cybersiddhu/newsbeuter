#include <tagsouppullparser.h>
#include <exceptions.h>
#include <utils.h>
#include <logger.h>
#include <stdexcept>
#include <istream>
#include <sstream>
#include <iostream>
#include <cstdlib>

#include <config.h>

namespace newsbeuter
{

/*
 * This method implements an "XML" pull parser. In reality, it's more liberal
 * than any XML pull parser, as it basically accepts everything that even only
 * remotely looks like XML. We use this parser for the HTML renderer.
 */

tagsouppullparser::tagsouppullparser() : inputstream(0), current_event(START_DOCUMENT)
{
}

tagsouppullparser::~tagsouppullparser()
{
}

void tagsouppullparser::setInput(std::istream& is) {
	inputstream = &is;
	current_event = START_DOCUMENT;
}

std::string tagsouppullparser::getAttributeValue(const std::string& name) const {
	for (std::vector<attribute>::const_iterator it=attributes.begin();it!=attributes.end();++it) {
		if (it->first == name) {
			return it->second;
		}	
	}	
	throw std::invalid_argument(_("attribute not found"));
}

tagsouppullparser::event tagsouppullparser::getEventType() const {
	return current_event;
}

std::string tagsouppullparser::getText() const {
	return text;
}

tagsouppullparser::event tagsouppullparser::next() {
	/*
	 * the next() method returns the next event by parsing the
	 * next element of the XML stream, depending on the current
	 * event.
	 */
	attributes.clear();
	text = "";

	if (inputstream->eof()) {
		current_event = END_DOCUMENT;
	}

	switch (current_event) {
		case START_DOCUMENT: 
		case START_TAG:
		case END_TAG:
			skip_whitespace();
			if (inputstream->eof()) {
				current_event = END_DOCUMENT;
				break;
			}
			if (c != '<') {
				handle_text();
				break;
			}
		case TEXT:
			handle_tag();
			break;
		case END_DOCUMENT:
			break;
	}
	return getEventType();	
}

void tagsouppullparser::skip_whitespace() {
	c = '\0';
	ws = "";
	do {
		inputstream->read(&c,1);
		if (!inputstream->eof()) {
			if (!isspace(c))
				break;
			else
				ws.append(1,c);
		}
	} while (!inputstream->eof());
}

void tagsouppullparser::add_attribute(std::string s) {
	if (s.length() > 0 && s[s.length()-1] == '/')
		s.erase(s.length()-1,1);
	if (s.length() == 0)
		return;
	std::string::size_type equalpos = s.find_first_of("=",0);
	std::string attribname, attribvalue;
	
	if (equalpos != std::string::npos) {
		attribname = s.substr(0,equalpos);
		attribvalue = s.substr(equalpos+1,s.length()-(equalpos+1));
	} else {
		attribname = attribvalue = s;
	}
	attribvalue = decode_attribute(attribvalue);
	attributes.push_back(attribute(attribname,attribvalue));
}

std::string tagsouppullparser::read_tag() {
	std::string s;
	getline(*inputstream,s,'>');
	if (inputstream->eof()) {
		throw xmlexception(_("EOF found while reading XML tag"));	// TODO: test whether this works reliably
	}
	return s;
}

tagsouppullparser::event tagsouppullparser::determine_tag_type() {
	if (text.length() > 0 && text[0] == '/') {
		text.erase(0,1);
		return END_TAG;
	}
	return START_TAG;	
}

std::string tagsouppullparser::decode_attribute(const std::string& s) {
	std::string s1 = s;
	if ((s1[0] == '"' && s1[s1.length()-1] == '"') || (s1[0] == '\'' && s1[s1.length()-1] == '\'')) {
		if (s1.length() > 0)
			s1.erase(0,1);
		if (s1.length() > 0)
			s1.erase(s1.length()-1,1);	
	}
	return decode_entities(s1);
}

std::string tagsouppullparser::decode_entities(const std::string& s) {
	std::string result, current_entity;
	std::istringstream sbuf(s);
	std::string tmp;
	getline(sbuf,tmp,'&');
	while (!sbuf.eof()) {
		result.append(tmp);
		getline(sbuf,tmp,';');
		result.append(decode_entity(tmp));
		getline(sbuf,tmp,'&');
	}
	result.append(tmp);
	return result;
}

static struct {
	const char * entity;
	unsigned int value;
} entity_table[] = {
	{ "quot", 34 },
	{ "bdquo", 34 },
	{ "rdquo", 34 },
	{ "amp", 38 },
	{ "apos", 39 },
	{ "lt", 60 },
	{ "gt", 62 },
	// this table was created with some vim regex magic from the this list: http://www.ramsch.org/martin/uni/fmi-hp/iso8859-1.html
	{ "nbsp", 160 },
	{ "iexcl", 161 },
	{ "cent", 162 },
	{ "pound", 163 },
	{ "curren", 164 },
	{ "yen", 165 },
	{ "brvbar", 166 },
	{ "sect", 167 },
	{ "uml", 168 },
	{ "copy", 169 },
	{ "ordf", 170 },
	{ "laquo", 171 },
	{ "not", 172 },
	{ "shy", 173 },
	{ "reg", 174 },
	{ "macr", 175 },
	{ "deg", 176 },
	{ "plusmn", 177 },
	{ "sup2", 178 },
	{ "sup3", 179 },
	{ "acute", 180 },
	{ "micro", 181 },
	{ "para", 182 },
	{ "middot", 183 },
	{ "cedil", 184 },
	{ "sup1", 185 },
	{ "ordm", 186 },
	{ "raquo", 187 },
	{ "frac14", 188 },
	{ "frac12", 189 },
	{ "frac34", 190 },
	{ "iquest", 191 },
	{ "Agrave", 192 },
	{ "Aacute", 193 },
	{ "Acirc", 194 },
	{ "Atilde", 195 },
	{ "Auml", 196 },
	{ "Aring", 197 },
	{ "AElig", 198 },
	{ "Ccedil", 199 },
	{ "Egrave", 200 },
	{ "Eacute", 201 },
	{ "Ecirc", 202 },
	{ "Euml", 203 },
	{ "Igrave", 204 },
	{ "Iacute", 205 },
	{ "Icirc", 206 },
	{ "Iuml", 207 },
	{ "ETH", 208 },
	{ "Ntilde", 209 },
	{ "Ograve", 210 },
	{ "Oacute", 211 },
	{ "Ocirc", 212 },
	{ "Otilde", 213 },
	{ "Ouml", 214 },
	{ "times", 215 },
	{ "Oslash", 216 },
	{ "Ugrave", 217 },
	{ "Uacute", 218 },
	{ "Ucirc", 219 },
	{ "Uuml", 220 },
	{ "Yacute", 221 },
	{ "THORN", 222 },
	{ "szlig", 223 },
	{ "agrave", 224 },
	{ "aacute", 225 },
	{ "acirc", 226 },
	{ "atilde", 227 },
	{ "auml", 228 },
	{ "aring", 229 },
	{ "aelig", 230 },
	{ "ccedil", 231 },
	{ "egrave", 232 },
	{ "eacute", 233 },
	{ "ecirc", 234 },
	{ "euml", 235 },
	{ "igrave", 236 },
	{ "iacute", 237 },
	{ "icirc", 238 },
	{ "iuml", 239 },
	{ "eth", 240 },
	{ "ntilde", 241 },
	{ "ograve", 242 },
	{ "oacute", 243 },
	{ "ocirc", 244 },
	{ "otilde", 245 },
	{ "ouml", 246 },
	{ "divide", 247 },
	{ "oslash", 248 },
	{ "ugrave", 249 },
	{ "uacute", 250 },
	{ "ucirc", 251 },
	{ "uuml", 252 },
	{ "yacute", 253 },
	{ "thorn", 254 },
	{ "yuml", 255 },
	/* more entities */
	{ "mdash", 8212 },
	{ "ndash", 8211 },
	{ 0, 0 }
};

std::string tagsouppullparser::decode_entity(std::string s) {
	LOG(LOG_DEBUG, "tagsouppullparser::decode_entity: decoding '%s'...", s.c_str());
	if (s.length() > 1 && s[0] == '#') {
		std::string result;
		unsigned int wc;
		char mbc[MB_CUR_MAX];
		mbc[0] = '\0';
		if (s[1] == 'x') {
			s.erase(0,2);
			std::istringstream is(s);
			is >> std::hex >> wc;
		} else {
			s.erase(0,1);
			std::istringstream is(s);
			is >> wc;
		}
		int pos;
		// convert some common but unknown numeric entities
		switch (wc) {
			case 133: wc = 8230; break; // &hellip;
			case 134: wc = 8224; break; // &dagger;
			case 135: wc = 8225; break; // &Dagger; (double dagger)
			case 150: wc = 8211; break; // &ndash;
			case 151: wc = 8212; break; // &mdash;
			case 152: wc =  732; break; // &tilde;
			case 153: wc = 8482; break; // &trade;
			case 156: wc =  339; break; // &oelig;
		}

		// std::cerr << "value: " << wc << " " << static_cast<wchar_t>(wc) << " pos: " << pos << std::endl;
		pos = wctomb(mbc,static_cast<wchar_t>(wc));
		if (pos > 0) {
			mbc[pos] = '\0';
			result.append(mbc);
		}
		LOG(LOG_DEBUG,"tagsouppullparser::decode_entity: wc = %u pos = %d mbc = '%s'", wc, pos, mbc);
		return result;
	} else {
		for (unsigned int i=0;entity_table[i].entity;++i) {
			if (s == entity_table[i].entity) {
				char mbc[MB_CUR_MAX];
				int pos = wctomb(mbc, entity_table[i].value);
				mbc[pos] = '\0';
				return std::string(mbc);
			}
		}
	}
	return ""; 	
}

void tagsouppullparser::remove_trailing_whitespace(std::string& s) {
	while (s.length() > 0 && isspace(s[s.length()-1])) {
		s.erase(s.length()-1,1);
	}
}

void tagsouppullparser::parse_tag(const std::string& tagstr) {
	std::string::size_type last_pos = tagstr.find_first_not_of(" \r\n\t", 0);
	std::string::size_type pos = tagstr.find_first_of(" \r\n\t", last_pos);
	unsigned int count = 0;

	LOG(LOG_DEBUG, "parse_tag: parsing '%s', pos = %d, last_pos = %d", tagstr.c_str(), pos, last_pos);

	while (last_pos != std::string::npos) {
		if (count == 0) {
			// first token: tag name
			if (pos == std::string::npos)
				pos = tagstr.length();
			text = tagstr.substr(last_pos, pos - last_pos);
			LOG(LOG_DEBUG, "parse_tag: tag name = %s", text.c_str());
		} else {
			pos = tagstr.find_first_of("= ", last_pos);
			std::string attr;
			if (pos != std::string::npos) {
				LOG(LOG_DEBUG, "parse_tag: found = or space");
				if (tagstr[pos] == '=') {
					LOG(LOG_DEBUG, "parse_tag: found =");
					if (tagstr[pos+1] == '\'' || tagstr[pos+1] == '"') {
						pos = tagstr.find_first_of("'\"", pos+2);
						if (pos != std::string::npos)
							pos++;
						LOG(LOG_DEBUG, "parse_tag: finding ending quote, pos = %d", pos);
					} else {
						pos = tagstr.find_first_of(" \r\n\t", pos+1);
						LOG(LOG_DEBUG, "parse_tag: finding end of unquoted attribute");
					}
				}
			}
			if (pos == std::string::npos) {
				LOG(LOG_DEBUG, "parse_tag: found end of string, correcting end position");
				pos = tagstr.length();
			}
			attr = tagstr.substr(last_pos, pos - last_pos);
			LOG(LOG_DEBUG, "parse_tag: extracted attribute is '%s', adding", attr.c_str());
			add_attribute(attr);
		}
		last_pos = tagstr.find_first_not_of(" \r\n\t", pos);
		count++;
	}
}

void tagsouppullparser::handle_tag() {
	std::string s;
	try {
		s = read_tag();
	} catch (const xmlexception &) {
		current_event = END_DOCUMENT;
		return;
	}
	parse_tag(s);
	current_event = determine_tag_type();
}

void tagsouppullparser::handle_text() {
	if (current_event != START_DOCUMENT) 
		text.append(ws);
	text.append(1,c);
	std::string tmp;
	getline(*inputstream,tmp,'<');
	text.append(tmp);
	text = decode_entities(text);
	current_event = TEXT;
}

}
