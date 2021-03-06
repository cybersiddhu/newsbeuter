#include <htmlrenderer.h>
#include <tagsouppullparser.h>
#include <utils.h>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <logger.h>
#include <libgen.h>
#include <config.h>

namespace newsbeuter {

htmlrenderer::htmlrenderer(unsigned int width, bool raw) : w(width), raw_(raw) { 
	tags["a"] = TAG_A;
	tags["embed"] = TAG_EMBED;
	tags["br"] = TAG_BR;
	tags["pre"] = TAG_PRE;
	tags["ituneshack"] = TAG_ITUNESHACK;
	tags["img"] = TAG_IMG;
	tags["blockquote"] = TAG_BLOCKQUOTE;
	tags["p"] = TAG_P;
	tags["h1"] = TAG_H1;
	tags["h2"] = TAG_H2;
	tags["h3"] = TAG_H3;
	tags["h4"] = TAG_H4;
	tags["ol"] = TAG_OL;
	tags["ul"] = TAG_UL;
	tags["li"] = TAG_LI;
	tags["dt"] = TAG_DT;
	tags["dd"] = TAG_DD;
	tags["dl"] = TAG_DL;
	tags["sup"] = TAG_SUP;
	tags["sub"] = TAG_SUB;
	tags["hr"] = TAG_HR;
	tags["b"] = TAG_STRONG;
	tags["strong"] = TAG_STRONG;
	tags["u"] = TAG_UNDERLINE;
}

void htmlrenderer::render(const std::string& source, std::vector<std::string>& lines, std::vector<linkpair>& links, const std::string& url) {
	std::istringstream input(source);
	render(input, lines, links, url);
}


unsigned int htmlrenderer::add_link(std::vector<linkpair>& links, const std::string& link, link_type type) {
	bool found = false;
	unsigned int i=1;
	for (std::vector<linkpair>::iterator it=links.begin();it!=links.end();++it, ++i) {
		if (it->first == link) {
			found = true;
			break;
		}
	}
	if (!found)
		links.push_back(linkpair(link,type));

	return i;
}

void htmlrenderer::render(std::istream& input, std::vector<std::string>& lines, std::vector<linkpair>& links, const std::string& url) {
	unsigned int image_count = 0;
	std::string curline;
	int indent_level = 0;
	bool inside_list = false, inside_li = false, is_ol = false, inside_pre = false;
	bool itunes_hack = false;
	unsigned int ol_count = 1;
	htmltag current_tag;
	int link_num = -1;
	
	/*
	 * to render the HTML, we use a self-developed "XML" pull parser.
	 *
	 * A pull parser works like this:
	 *   - we feed it with an XML stream
	 *   - we then gather an iterator
	 *   - we then can iterate over all continuous elements, such as start tag, close tag, text element, ...
	 */
	tagsouppullparser xpp;
	xpp.setInput(input);
	
	for (tagsouppullparser::event e = xpp.next(); e != tagsouppullparser::END_DOCUMENT; e = xpp.next()) {	
		std::string tagname;
		switch (e) {
			case tagsouppullparser::START_TAG:
				tagname = xpp.getText();
				std::transform(tagname.begin(), tagname.end(), tagname.begin(), ::tolower);
				current_tag = tags[tagname];

				switch (current_tag) {
					case TAG_A: {
							std::string link;
							try {
								link = xpp.getAttributeValue("href");
							} catch (const std::invalid_argument& ) {
								LOG(LOG_WARN,"htmlrenderer::render: found a tag with no href attribute");
								link = "";
							}
							if (link.length() > 0) {
								link_num = add_link(links,utils::censor_url(utils::absolute_url(url,link)), LINK_HREF);
								if (!raw_) 
									curline.append("<u>");
							}
						}
						break;
					case TAG_STRONG:
						if (!raw_) 
							curline.append("<b>");
						break;
					case TAG_UNDERLINE:
						if (!raw_) 
							curline.append("<u>");
						break;

					case TAG_EMBED: {
							std::string type;
							try {
								type = xpp.getAttributeValue("type");
							} catch (const std::invalid_argument& ) {
								LOG(LOG_WARN, "htmlrenderer::render: found embed object without type attribute");
								type = "";
							}
							if (type == "application/x-shockwave-flash") {
								std::string link;
								try {
									link = xpp.getAttributeValue("src");
								} catch (const std::invalid_argument& ) {
									LOG(LOG_WARN, "htmlrenderer::render: found embed object without src attribute");
									link = "";
								}
								if (link.length() > 0) {
									unsigned int link_num = add_link(links,utils::censor_url(utils::absolute_url(url,link)), LINK_EMBED);
									curline.append(utils::strprintf("[%s %u]", _("embedded flash:"), link_num));
								}
							}
						}
						break;

					case TAG_BR:
						lines.push_back(curline);
						prepare_newline(curline, indent_level);	
						break;

					case TAG_PRE:
						inside_pre = true;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);	
						break;

					case TAG_ITUNESHACK:
						itunes_hack = true;
						break;

					case TAG_IMG: {
							std::string imgurl;
							try {
								imgurl = xpp.getAttributeValue("src");
							} catch (const std::invalid_argument& ) {
								LOG(LOG_WARN,"htmlrenderer::render: found img tag with no src attribute");
								imgurl = "";
							}
							if (imgurl.length() > 0) {
								unsigned int link_num = add_link(links,utils::censor_url(utils::absolute_url(url,imgurl)), LINK_IMG);
								curline.append(utils::strprintf("[%s %u]", _("image"), link_num));
								image_count++;
							}
						}
						break;

					case TAG_BLOCKQUOTE:
						++indent_level;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_H1:
					case TAG_H2:
					case TAG_H3:
					case TAG_H4:
					case TAG_P:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						if (lines.size() > 0 && lines[lines.size()-1].length() > static_cast<unsigned int>(indent_level*2))
							lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_OL:
						inside_list = true;
						is_ol = true;
						ol_count = 1;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_UL:
						inside_list = true;
						is_ol = false;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);
						break;

					case TAG_LI:
						if (inside_li) {
							indent_level-=2;
							if (indent_level < 0) indent_level = 0;
							if (line_is_nonempty(curline))
								lines.push_back(curline);
							prepare_newline(curline, indent_level);	
						}
						inside_li = true;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						indent_level+=2;
						if (is_ol) {
							curline.append(utils::strprintf("%2u.", ol_count));
							++ol_count;
						} else {
							curline.append("  * ");
						}
						break;

					case TAG_DT:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						break;

					case TAG_DD:
						indent_level+=4;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						break;

					case TAG_DL:
						// ignore tag
						break;

					case TAG_SUP:
						curline.append("^");
						break;

					case TAG_SUB:
						curline.append("[");
						break;

					case TAG_HR:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						lines.push_back(std::string(" ") + std::string(w - 2, '-') + std::string(" "));
						prepare_newline(curline, indent_level);
						break;
					default:
						break;
				}
				break;

			case tagsouppullparser::END_TAG:
				tagname = xpp.getText();
				std::transform(tagname.begin(), tagname.end(), tagname.begin(), ::tolower);
				current_tag = tags[tagname];

				switch (current_tag) {
					case TAG_BLOCKQUOTE:
						--indent_level;
						if (indent_level < 0) indent_level = 0;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);
						break;

					case TAG_OL:
					case TAG_UL:
						inside_list = false;
						if (inside_li) {
							indent_level-=2;
							if (indent_level < 0) indent_level = 0;
							if (line_is_nonempty(curline))
								lines.push_back(curline);
							prepare_newline(curline, indent_level);
						}
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_DT:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_DD:
						indent_level-=4;
						if (indent_level < 0) indent_level = 0;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						lines.push_back("");
						prepare_newline(curline, indent_level);	
						break;

					case TAG_DL:
						// ignore tag
						break;

					case TAG_LI:
						indent_level-=2;
						if (indent_level < 0) indent_level = 0;
						inside_li = false;
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						break;

					case TAG_H1:
						if (line_is_nonempty(curline)) {
							lines.push_back(curline);
							size_t llen = utils::strwidth(curline);
							prepare_newline(curline, indent_level);
							lines.push_back(std::string(llen, '-'));
						}
						prepare_newline(curline, indent_level);
						break;

					case TAG_H2:
					case TAG_H3:
					case TAG_H4:
					case TAG_P:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						break;

					case TAG_PRE:
						if (line_is_nonempty(curline))
							lines.push_back(curline);
						prepare_newline(curline, indent_level);
						inside_pre = false;
						break;

					case TAG_SUB:
						curline.append("]");
						break;
					
					case TAG_SUP:
						// has closing tag, but we render nothing.
						break;

					case TAG_A:
						if (link_num != -1) {
							if (!raw_)
								curline.append("</>");
							curline.append(utils::strprintf("[%d]", link_num));
							link_num = -1;
						}
						break;

					case TAG_UNDERLINE:
						if (!raw_)
							curline.append("</>");
						break;

					case TAG_STRONG:
						if (!raw_)
							curline.append("</>");
						break;

					case TAG_EMBED:
					case TAG_BR:
					case TAG_ITUNESHACK:
					case TAG_IMG:
					case TAG_HR:
						// ignore closing tags
						break;
					default:
						break;
				}
				break;

			case tagsouppullparser::TEXT:
				{
					if (itunes_hack) {
						std::vector<std::string> words = utils::tokenize_nl(utils::quote_for_stfl(xpp.getText()));
						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it) {
							if (*it == "\n") {
								lines.push_back(curline);
								prepare_newline(curline, indent_level);
							} else {
								std::vector<std::string> words2 = utils::tokenize_spaced(*it);
								unsigned int i=0;
								bool new_line = false;
								for (std::vector<std::string>::iterator it2=words2.begin();it2!=words2.end();++it2,++i) {
									if ((curline.length() + it2->length()) >= w) {
										if (line_is_nonempty(curline))
											lines.push_back(curline);
										prepare_newline(curline, indent_level);
										new_line = true;
									}
									if (new_line) {
										if (*it2 != " ")
											curline.append(*it2);
										new_line = false;
									} else {
										curline.append(*it2);
									}
								}
							}
						}
					} else if (inside_pre) {
						std::vector<std::string> words = utils::tokenize_nl(utils::quote_for_stfl(xpp.getText()));
						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it) {
							if (*it == "\n") {
								lines.push_back(curline);
								prepare_newline(curline, indent_level);
							} else {
								curline.append(*it);
							}
						}
					} else {
						std::string s = utils::quote_for_stfl(xpp.getText());
						while (s.length() > 0 && s[0] == '\n')
							s.erase(0, 1);
						std::vector<std::string> words = utils::tokenize_spaced(s);

						unsigned int i=0;
						bool new_line = false;

						if (!line_is_nonempty(curline) && words.size() > 0 && words[0] == " ") {
							words.erase(words.begin());
						}

						for (std::vector<std::string>::iterator it=words.begin();it!=words.end();++it,++i) {
							if ((curline.length() + it->length()) >= w) {
								if (line_is_nonempty(curline))
									lines.push_back(curline);
								prepare_newline(curline, indent_level);
								new_line = true;
							}
							if (new_line) {
								if (*it != " ")
									curline.append(*it);
								new_line = false;
							} else {
								curline.append(*it);
							}
						}
					}
				}
				break;
			default:
				/* do nothing */
				break;
		}
	}
	if (line_is_nonempty(curline))
		lines.push_back(curline);
	
	if (links.size() > 0) {
		lines.push_back("");
		lines.push_back(_("Links: "));
		for (unsigned int i=0;i<links.size();++i) {
			lines.push_back(utils::strprintf("[%u]: %s (%s)", i+1, links[i].first.c_str(), type2str(links[i].second).c_str()));
		}
	}
}

std::string htmlrenderer::type2str(link_type type) {
	switch (type) {
		case LINK_HREF: return _("link");
		case LINK_IMG: return _("image");
		case LINK_EMBED: return _("embedded flash");
		default: return _("unknown (bug)");
	}
}

void htmlrenderer::prepare_newline(std::string& line, int indent_level) {
	line = "";
	line.append(indent_level*2, ' ');
}

bool htmlrenderer::line_is_nonempty(const std::string& line) {
	for (unsigned int i=0;i<line.length();++i) {
		if (!isblank(line[i]) && line[i] != '\n' && line[i] && '\r')
			return true;
	}
	return false;
}

}
