#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <lexbor/html/html.h>


//
// The lxb namespace provides C++ utilities over the lexbor library.
//
// - lxb::string is std::string for lxb_char's
// - lxb::txt_content() extracts a string from a DOM element
// - lxb::pointer<> is a unique pointer that knows how to delete various
//   lexbor pointers
// - lxb::element_collection wraps a lxb_dom_collection_t in something
//   that resembles std::vector<lxb_dom_element_t*>
// - find_elements_by_class_name finds zero or more DOM elements by
//   CSS class name and returns the collection
// - find_element_by_class_name finds zero or one DOM elements by CSS
//   class name and returns a pointer to the element
//
// https://lexbor.com
//
namespace lxb {

typedef std::basic_string<lxb_char_t> string;

string
text_content(lxb_dom_element_t *element)
{
	size_t buf_len;
	lxb_char_t *ptr = lxb_dom_node_text_content(&element->node, &buf_len);
	if (buf_len == 0) {
		return string();
	}
	return string(ptr);
}

template<typename T> void destroy(T *ptr);

template<>
void
destroy<>(lxb_html_document_t *ptr)
{
	BOOST_LOG_TRIVIAL(debug) << "destroying lxb_html_document_t " << ptr;
	lxb_html_document_destroy(ptr);
}

template<>
void
destroy<>(lxb_html_parser_t *ptr)
{
	BOOST_LOG_TRIVIAL(debug) << "destroying lxb_html_parser_t " << ptr;
	lxb_html_parser_destroy(ptr);
}

template<>
void
destroy<>(lxb_dom_collection_t *ptr)
{
	BOOST_LOG_TRIVIAL(debug) << "destroying lxb_dom_collection_t " << ptr;
	lxb_dom_collection_destroy(ptr, true);
}

template<typename T>
struct deleter {
	void operator()(T *ptr) {
		if (ptr) {
			lxb::destroy(ptr);
		}
	}
};

template<typename T>
using pointer = std::unique_ptr<T, lxb::deleter<T>>;

class element_collection {
public:
	element_collection() = default;

	explicit element_collection(pointer<lxb_dom_collection_t> &data) {
		m_data = std::move(data);
		for (size_t idx=0,
			    len=lxb_dom_collection_length(m_data.get());
		     idx < len; ++idx)
		{
			m_elements.push_back(lxb_dom_collection_element(
				m_data.get(), idx));
		}
	}

	bool empty() const {
		return m_elements.empty();
	}

	size_t size() const {
		return m_elements.size();
	}

	lxb_dom_element* at(size_t index) {
		return m_elements.at(index);
	}

	std::vector<lxb_dom_element_t *>::iterator begin() {
		return m_elements.begin();
	}

	std::vector<lxb_dom_element_t *>::iterator end() {
		return m_elements.end();
	}

private:
	pointer<lxb_dom_collection_t> m_data;
	std::vector<lxb_dom_element_t *> m_elements;
};

element_collection
find_elements_by_class_name(lxb_dom_element_t *root,
	                    std::string const &class_name,
			    size_t size_hint=10)
{
	lxb_dom_document_t *doc = root->node.owner_document;
	pointer<lxb_dom_collection_t> raw;
	raw.reset(lxb_dom_collection_make(doc, size_hint));
	lxb_status_t rc = lxb_dom_elements_by_class_name(
		root, raw.get(), (lxb_char_t const *) class_name.c_str(),
		class_name.length());
	if (rc != LXB_STATUS_OK) {
		BOOST_LOG_TRIVIAL(debug)
			<< "failed to find elements with class "
			<< class_name;
		return element_collection();
	}
	return element_collection(raw);
}

lxb_dom_element_t*
find_element_by_class_name(lxb_dom_element_t *root,
	                   std::string const& class_name)
{
	auto matches = find_elements_by_class_name(root, class_name);
	if (!matches.empty()) {
		return matches.at(0);
	}
	BOOST_LOG_TRIVIAL(debug)
		<< "failed to find element with class "
		<< class_name;
	return nullptr;
}

lxb_dom_element_t*
find_element_by_class_name(pointer<lxb_html_document_t>& doc,
	                   std::string const& class_name)
{
	lxb_dom_document_t *dom_doc = lxb_dom_interface_document(doc.get());
	lxb_dom_element_t *dom_root = lxb_dom_document_element(dom_doc);
	return find_element_by_class_name(dom_root, class_name);
}

} // end lxb namespace


// Thrown when the curl_client fails.
class curl_exception: public std::exception {
public:
	explicit curl_exception(CURLcode code): m_code(code) {
		std::ostringstream oss;
		oss << "curl failure: " << curl_easy_strerror(m_code)
		    << " (" << m_code << ")";
		m_message.assign(oss.str());
	}
	curl_exception(char const *msg, CURLcode code): m_code(code) {
		std::ostringstream oss;
		oss << msg << ": " << curl_easy_strerror(m_code)
		    << " (" << m_code << ")";
		m_message.assign(oss.str());
	}
	char const* what() const noexcept override {
		return m_message.c_str();
	}
	CURLcode code() const noexcept { return m_code; }

	// Simple function that throws if code != OK
	static void fail_if_error(char const *msg, CURLcode code) {
		if (code != CURLE_OK) {
			throw curl_exception(msg, code);
		}
	}

private:
	std::string m_message;
	CURLcode m_code;
};

//
// Wrapper around the C-based CURL API.
//
class curl_client {
public:
	curl_client() {
		if ((m_handle=curl_easy_init()) == nullptr) {
			throw std::bad_alloc();
		}

		CURLcode rc;

		rc = curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,
			              this->write_callback);
		curl_exception::fail_if_error("failed to set option", rc);

		rc = curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this);
		curl_exception::fail_if_error("failed to set option", rc);
	}
	~curl_client() { curl_easy_cleanup(m_handle); }

	// Retrieve URL and return its body in `buffer`.
	// Return value is the HTTP status code.
	int fetch(std::string const& url, std::string& buffer) {
		CURLcode rc;

		BOOST_LOG_TRIVIAL(info) << "retrieving " << url;
		m_buffer.clear();
		rc = curl_easy_setopt(m_handle, CURLOPT_URL, url.c_str());
		curl_exception::fail_if_error("failed to set URL", rc);

		rc = curl_easy_perform(m_handle);
		if (rc == CURLE_OK) {
			BOOST_LOG_TRIVIAL(info)
				<< "retrieved " << m_buffer.size()
				<< " bytes from " << url;
			std::move(m_buffer.begin(), m_buffer.end(),
				std::back_inserter(buffer));
		} else {
			BOOST_LOG_TRIVIAL(error)
				<< "failed to fetch " << url << ": "
				<< curl_easy_strerror(rc)
				<< " (" << rc << ")";
		}
		return rc;
	}

private:
	static size_t write_callback(char const *data, size_t len, size_t num,
		                     void *context)
	{
		auto *self = reinterpret_cast<curl_client*>(context);
		if (self->m_status_code < 0) {
                        curl_easy_getinfo(
                                self->m_handle, CURLINFO_RESPONSE_CODE,
                                &self->m_status_code);
			if (self->m_status_code >= 400) {
				BOOST_LOG_TRIVIAL(warning)
					<< "remote server failure: HTTP "
					<< self->m_status_code
					<< ", terminating.";
				return (size_t)-1;
			}
		}
		BOOST_LOG_TRIVIAL(debug)
			<< "received " << (len * num) << " bytes";
		std::copy_n(data, len * num,
		            std::back_inserter(self->m_buffer));
		return len * num;
	}

	CURL *m_handle;
	long m_status_code = -1;
	std::vector<char> m_buffer;
};


//
// Helpers to format lexbor values
//
std::ostream &
operator<<(std::ostream &os, lxb_dom_attr_t *const &attr)
{
	os << lexbor_str_data(attr->value);
	return os;
}

std::ostream &
operator<<(std::ostream &os, lxb::string const &s)
{
	os << (char *) s.c_str();
	return os;
}


//
// Parse a document from a stream into a Lexbor HTML document
// This throws exceptions for failures.
//
lxb::pointer<lxb_html_document_t>
parse_document(std::basic_istringstream<char> input)
{
	lxb::pointer<lxb_html_parser_t> parser(lxb_html_parser_create());
	if (lxb_html_parser_init(parser.get()) != LXB_STATUS_OK) {
		throw std::runtime_error("failed to initialize parser");
	}

	lxb::pointer<lxb_html_document_t> document(
		lxb_html_parse_chunk_begin(parser.get()));
	if (!document) {
		throw std::runtime_error("failed to create document");
	}

	int chunk_counter;
	std::vector<lxb_char_t> buffer(4096, 0);
	for (chunk_counter = 0;
	     input.read((char *) buffer.data(), buffer.size());
	     chunk_counter++)
	{
		lxb_html_parse_chunk_process(parser.get(), &buffer[0],
					     buffer.size());
		if (parser->status != LXB_STATUS_OK) {
			throw std::runtime_error("chunk process failed");
		}
	}
	lxb_html_parse_chunk_end(parser.get());
	if (parser->status != LXB_STATUS_OK) {
		throw std::runtime_error("chunk finalization failed");
	}

	BOOST_LOG_TRIVIAL(info)
		<< "processed input in " << chunk_counter << " chunks";

	return document;
}


//
// Parse command-line parameters.
//
void
parse_arguments(int argc, char **argv,
	        std::string *url, std::string *output_filename,
	        bool *verbose)
{
	namespace po = boost::program_options;

	po::options_description visible_options("Options");
	visible_options.add_options()
		("help,h", "produce help message")
		("verbose,v",
		 po::bool_switch(verbose)->default_value(false),
		 "enable diagnostic output")
		;

	po::options_description posn_desc("");
	posn_desc.add_options()
		("url", po::value<std::string>(url), "URL to retrieve")
		("output-file", po::value<std::string>(output_filename),
		 "name of the file to write")
		;

	po::positional_options_description positional;
	positional.add("url", 1);
	positional.add("output-file", 1);

	po::options_description cli_options("");
	cli_options.add(visible_options);
	cli_options.add(posn_desc);
	auto parser = po::command_line_parser(argc, argv)
				.options(cli_options)
				.positional(positional);
	po::variables_map vm;
	try {
		po::store(parser.run(), vm);
	} catch (po::unknown_option const& error) {
		BOOST_LOG_TRIVIAL(error)
			<< "failed to parse arguments: "
			<< error.what();
		std::exit(64);
	}
	po::notify(vm);
	if (vm.count("help") || vm["url"].empty()
	    || vm["output-file"].empty())
	{
		std::cout << "Usage: " << argv[0]
		          << " [options] URL OUTPUT-FILE" << std::endl
		          << visible_options << std::endl;
		std::exit(64);
	}
}


int
main(int argc, char **argv)
{
	namespace logging = boost::log;

	std::string url, output_filename;
	bool verbose;

	parse_arguments(argc, argv, &url, &output_filename, &verbose);
	if (verbose) {
		logging::core::get()->set_filter(
			logging::trivial::severity >= logging::trivial::debug);
	} else {
		logging::core::get()->set_filter(
			logging::trivial::severity >= logging::trivial::info);
	}

	curl_client client;
	std::string html;
	if (client.fetch(url, html) != 0) {
		BOOST_LOG_TRIVIAL(error)
			<< "Failed to retrieve document from " << url;
		std::exit(-1);
	}

	lxb::pointer<lxb_html_document_t> doc;
	doc = parse_document(std::istringstream(html));

	lxb_dom_element_t *content_root = lxb::find_element_by_class_name(
		doc, "recipe__text__content");
	if (content_root == nullptr) {
		BOOST_LOG_TRIVIAL(error) << "failed to find content root";
		std::exit(-1);
	}


	std::ofstream output_file(output_filename);
	if (!output_file) {
		BOOST_LOG_TRIVIAL(error)
			<< "failed to open output file: " << output_filename
			<< ": " << strerror(errno) << " (" << errno << ")";
		std::exit(-1);
	}

	BOOST_LOG_TRIVIAL(info)
		<< "writing output to " << output_filename;

	auto title = lxb::find_element_by_class_name(
		doc, "recipe-header__title");

	output_file
		<< "<html><head><meta charset=utf-8><title>"
		<< lxb::text_content(title)
		<< "</title></head><body><h1>"
		<< lxb::text_content(title)
		<< "</h1><h2>Ingredients</h2>";

	auto ingredient_list = lxb::find_element_by_class_name(
		content_root, "ingredients-list");
	if (ingredient_list) {
		output_file << "<table>";
		auto ingredients = lxb::find_elements_by_class_name(
			ingredient_list, "ingredient");
		for (auto & ingredient : ingredients) {
			auto quantity = lxb::find_element_by_class_name(
				ingredient, "ingredient__quantity");
			auto label = lxb::find_element_by_class_name(
				ingredient, "ingredient__label");
			output_file
				<< "<tr><td>" << lxb::text_content(quantity)
				<< "</td><td>" << lxb::text_content(label)
				<< "</td></tr>";
		}
		output_file << "</table>";
	}

	output_file << "<h2>Directions</h2>";
	auto instruction_list = lxb::find_element_by_class_name(
		content_root, "recipe__directions__list");
	if (instruction_list) {
		output_file << "<ol>";
		auto instructions = lxb::find_elements_by_class_name(
			instruction_list, "recipe__direction__text");
		for (auto & instruction : instructions) {
			output_file
				<< "<li>" << lxb::text_content(instruction)
				<< "</li>";
		}
		output_file << "</ol>";
	}

	output_file
		<< "<p><i>Extracted from " << url
		<< "</i></p></body></html>";

	return 0;
}
