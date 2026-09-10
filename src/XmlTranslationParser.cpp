// AI CONTEXT: xTranslator XML parser implementation using XmlLite.
// Depends on XmlTranslationParser declarations and Windows XmlLite stream APIs.
// Runtime assumptions: version-neutral Fallout 4 record signatures without direct game access.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: original Source text is retained only as inert XML metadata.
#include "XmlTranslationParser.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <XmlLite.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <initializer_list>
#include <string_view>
#include <unordered_map>

namespace
{
	template <class T>
	class ComPtr
	{
	public:
		~ComPtr()
		{
			if (m_ptr)
			{
				m_ptr->Release();
			}
		}

		T** put()
		{
			return &m_ptr;
		}

		T* get() const
		{
			return m_ptr;
		}

		T* operator->() const
		{
			return m_ptr;
		}

	private:
		T* m_ptr{ nullptr };
	};

	std::string wideToUtf8(const wchar_t* input)
	{
		if (!input)
		{
			return {};
		}

		const auto size = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
		if (size <= 1)
		{
			return {};
		}

		std::string result(static_cast<std::size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, input, -1, result.data(), size, nullptr, nullptr);
		result.pop_back();
		return result;
	}

	std::optional<std::uint32_t> parseUInt(std::string_view value, int base)
	{
		if (value.empty())
		{
			return std::nullopt;
		}

		// Handle 0x/0X prefix for hex parsing
		auto start = value.data();
		auto len = value.size();
		if (base == 16 && len > 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
		{
			start += 2;
			len -= 2;
		}

		std::uint32_t parsed = 0;
		const auto* first = start;
		const auto* last = start + len;
		const auto result = std::from_chars(first, last, parsed, base);
		if (result.ec != std::errc{} || result.ptr != last)
		{
			return std::nullopt;
		}

		return parsed;
	}

	bool pathIs(const std::vector<std::string>& path, std::initializer_list<std::string_view> expected)
	{
		if (path.size() != expected.size())
		{
			return false;
		}

		auto pathIt = path.begin();
		auto expectedIt = expected.begin();
		for (; pathIt != path.end(); ++pathIt, ++expectedIt)
		{
			if (*pathIt != *expectedIt)
			{
				return false;
			}
		}

		return true;
	}

	std::string_view trim(std::string_view value)
	{
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		{
			value.remove_prefix(1);
		}
		while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		{
			value.remove_suffix(1);
		}
		return value;
	}

	std::string upper(std::string_view value)
	{
		std::string result{ value };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::toupper(ch));
		});
		return result;
	}

	std::string normalizeRecordSignature(std::string_view rec)
	{
		const auto pos = rec.find(':');
		if (pos == std::string_view::npos)
		{
			return {};
		}

		const auto record = upper(trim(rec.substr(0, pos)));
		const auto field = upper(trim(rec.substr(pos + 1)));
		if (record.size() != 4 || field.size() != 4)
		{
			return {};
		}

		return record + ' ' + field;
	}

	bool matchesRecordFilter(const XmlTranslationEntry& entry, std::string_view recordFilter)
	{
		return recordFilter.empty() || normalizeRecordSignature(entry.record) == recordFilter;
	}

	void readAttributes(IXmlReader* reader, XmlTranslationEntry& entry, bool recAttributes)
	{
		for (auto hr = reader->MoveToFirstAttribute(); hr == S_OK; hr = reader->MoveToNextAttribute())
		{
			if (reader->IsDefault())
			{
				continue;
			}

			const wchar_t* nameRaw = nullptr;
			const wchar_t* valueRaw = nullptr;
			reader->GetQualifiedName(&nameRaw, nullptr);
			reader->GetValue(&valueRaw, nullptr);

			const auto name = wideToUtf8(nameRaw);
			const auto value = wideToUtf8(valueRaw);

			if (recAttributes)
			{
				if (name == "id")
				{
					entry.index = parseUInt(value, 10);
				}
				else if (name == "idMax")
				{
					entry.indexMax = parseUInt(value, 10);
				}
			}
			else if (name == "List")
			{
				if (const auto parsed = parseUInt(value, 10))
				{
					entry.list = static_cast<int>(*parsed);
				}
			}
			else if (name == "sID")
			{
				entry.stringID = parseUInt(value, 16);
			}
			else if (name == "FormID")
			{
				entry.formID = parseUInt(value, 16);
			}
			else if (name == "Partial")
			{
				entry.partial = parseUInt(value, 10);
			}
		}

		reader->MoveToElement();
	}

	void appendText(XmlTranslationFile& file, XmlTranslationEntry& entry, const std::vector<std::string>& path, std::string_view text, std::string_view recordFilter)
	{
		if (pathIs(path, { "SSTXMLRessources", "Params", "Addon" }))
		{
			file.addon.append(text);
		}
		else if (pathIs(path, { "SSTXMLRessources", "Content", "String", "FormID" }))
		{
			entry.formID = parseUInt(text, 16);
		}
		else if (pathIs(path, { "SSTXMLRessources", "Content", "String", "EDID" }))
		{
			entry.edid.append(text);
		}
		else if (pathIs(path, { "SSTXMLRessources", "Content", "String", "REC" }))
		{
			entry.record.append(text);
		}
		else if (pathIs(path, { "SSTXMLRessources", "Content", "String", "Source" }))
		{
			if (recordFilter.empty() || entry.record.empty() || matchesRecordFilter(entry, recordFilter))
			{
				entry.source.append(text);
			}
		}
		else if (pathIs(path, { "SSTXMLRessources", "Content", "String", "Dest" }))
		{
			if (recordFilter.empty() || entry.record.empty() || matchesRecordFilter(entry, recordFilter))
			{
				entry.dest.append(text);
			}
		}
	}

	std::string makeRecordOrdinalKey(const XmlTranslationEntry& entry)
	{
		return entry.edid + '\x1F' + entry.record;
	}

	void finishEntry(XmlTranslationFile& file, XmlTranslationEntry& entry, std::unordered_map<std::string, std::size_t>& recordOrdinals, std::string_view recordFilter)
	{
		if (!matchesRecordFilter(entry, recordFilter))
		{
			return;
		}

		entry.ordinal = file.entries.size();
		auto& recordOrdinal = recordOrdinals[makeRecordOrdinalKey(entry)];
		entry.recordOrdinal = recordOrdinal++;
		file.entries.push_back(std::move(entry));
	}
}

namespace XmlTranslationParser
{
	XmlParseResult ParseFile(const std::filesystem::path& path)
	{
		return ParseFile(path, {});
	}

	XmlParseResult ParseFile(const std::filesystem::path& path, std::string_view recordFilter)
	{
		XmlParseResult result;
		result.file.path = path;

		ComPtr<IStream> stream;
		auto hr = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream.put());
		if (FAILED(hr))
		{
			result.error = "failed to open XML file";
			return result;
		}

		ComPtr<IXmlReader> reader;
		hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.put()), nullptr);
		if (FAILED(hr))
		{
			result.error = "failed to create XML reader";
			return result;
		}

		hr = reader->SetInput(stream.get());
		if (FAILED(hr))
		{
			result.error = "failed to initialize XML reader";
			return result;
		}

		std::vector<std::string> pathStack;
		std::unordered_map<std::string, std::size_t> recordOrdinals;
		std::optional<XmlTranslationEntry> current;
		std::string rootName;

		XmlNodeType nodeType{};
		while ((hr = reader->Read(&nodeType)) == S_OK)
		{
			switch (nodeType)
			{
			case XmlNodeType_Element:
			{
				const wchar_t* nameRaw = nullptr;
				reader->GetQualifiedName(&nameRaw, nullptr);
				const auto name = wideToUtf8(nameRaw);
				if (pathStack.empty())
				{
					rootName = name;
				}

				pathStack.push_back(name);

				const bool isString = pathIs(pathStack, { "SSTXMLRessources", "Content", "String" });
				const bool isRec = pathIs(pathStack, { "SSTXMLRessources", "Content", "String", "REC" });
				if (isString)
				{
					current.emplace();
					readAttributes(reader.get(), *current, false);
				}
				else if (isRec && current)
				{
					readAttributes(reader.get(), *current, true);
				}

				if (reader->IsEmptyElement())
				{
					if (isString && current)
					{
						finishEntry(result.file, *current, recordOrdinals, recordFilter);
						current.reset();
					}
					pathStack.pop_back();
				}
			}
			break;
			case XmlNodeType_Text:
			case XmlNodeType_CDATA:
			case XmlNodeType_Whitespace:
			{
				const wchar_t* valueRaw = nullptr;
				reader->GetValue(&valueRaw, nullptr);
				const auto value = wideToUtf8(valueRaw);
				if (current)
				{
					appendText(result.file, *current, pathStack, value, recordFilter);
				}
				else if (pathIs(pathStack, { "SSTXMLRessources", "Params", "Addon" }))
				{
					XmlTranslationEntry unused;
					appendText(result.file, unused, pathStack, value, recordFilter);
				}
			}
			break;
			case XmlNodeType_EndElement:
			{
				if (pathIs(pathStack, { "SSTXMLRessources", "Content", "String" }) && current)
				{
					finishEntry(result.file, *current, recordOrdinals, recordFilter);
					current.reset();
				}

				if (!pathStack.empty())
				{
					pathStack.pop_back();
				}
			}
			break;
			default:
				break;
			}
		}

		if (FAILED(hr))
		{
			result.error = "malformed XML";
			return result;
		}

		if (rootName != "SSTXMLRessources")
		{
			result.error = "missing SSTXMLRessources root";
			return result;
		}

		result.success = true;
		return result;
	}
}
