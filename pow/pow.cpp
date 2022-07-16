#include "rc/version.h"

#include <TermAPI.hpp>
#include <ParamsAPI2.hpp>
#include <envpath.hpp>
#include <str.hpp>

#include <regex>

struct Help {
	const std::string& programName;

	constexpr Help(const std::string& programName) : programName{ programName } {}

	friend std::ostream& operator<<(std::ostream& os, const Help& h)
	{
		return os
			<< "pow  v" << pow_VERSION_EXTENDED << '\n'
			<< "  Commandline exponent calculator." << '\n'
			<< '\n'
			<< "USAGE:\n"
			<< "  " << h.programName << " [OPTIONS] <N>^<EXP>" << '\n'
			<< '\n'
			<< "  All parameters that are not options are concatenated together before they are parsed." << '\n'
			<< "  Operations are delimited using commas ','." << '\n'
			<< '\n'
			<< "OPTIONS:\n"
			<< "  -h, --help             Shows this help display, then exits." << '\n'
			<< "  -v, --version          Shows the current version number, then exits." << '\n'
			<< "  -q, --quiet            Prevents non-essential console output & formatting." << '\n'
			<< "  -n, --no-color         Disables the use of ANSI color escape sequences in console output." << '\n'
			;
	}
};

template<typename T> concept valid_pow_type = requires()
{
	std::pow(std::declval<T>(), std::declval<T>());
};

struct Pow {
	std::string number, exponent;

protected:
	template<valid_pow_type T>
	T getResult(std::function<T(std::string)> const& converter) const
	{
		return std::pow(converter(number), converter(exponent));
	}

public:
	CONSTEXPR Pow() {}
	CONSTEXPR Pow(std::string const& number, std::string const& exponent) : number{ number }, exponent{ exponent } {}
	CONSTEXPR Pow(std::string&& number, std::string&& exponent) : number{ std::move(number) }, exponent{ std::move(exponent) } {}

	/// @brief	Returns true when the number or exponent should be parsed as a floating-point number.
	bool hasFloatingPoint() const
	{
		return number.find('.') < number.size() || exponent.find('.') < exponent.size();
	}
	bool hasNegative() const
	{
		static const std::string search_str{ "-" };
		return number.find_first_of(search_str) < number.find_first_not_of(search_str) || exponent.find_first_of(search_str) < exponent.find_first_not_of(search_str);
	}

	template<std::signed_integral TReturn>
	TReturn getResult() const
	{
		return static_cast<TReturn>(this->getResult<long long>(str::stoll));
	}
	template<std::unsigned_integral TReturn>
	TReturn getResult() const
	{
		return static_cast<TReturn>(this->getResult<long long>(str::stoull));
	}
	template<std::floating_point TReturn>
	TReturn getResult() const
	{
		return static_cast<TReturn>(this->getResult<long double>(str::stold));
	}

	template<var::Streamable... Ts>
	std::string getResultString(Ts&&... stream_formatting) const
	{
		if (hasFloatingPoint())
			return str::stringify(std::forward<Ts>(stream_formatting)..., getResult<long double>());
		else if (hasNegative())
			return str::stringify(std::forward<Ts>(stream_formatting)..., getResult<long long>());
		else
			return str::stringify(std::forward<Ts>(stream_formatting)..., getResult<unsigned long long>());
	}
};

enum class COLOR : unsigned char {
	NUMBER,
	EXPONENT,
	RESULT,
	CARET,
	EQUALS,
	BRACKET,
};

static struct {
	bool quiet{ false };
	term::palette<COLOR> colors{
		std::make_pair(COLOR::NUMBER, color::yellow),
		std::make_pair(COLOR::EXPONENT, color::yellow),
		std::make_pair(COLOR::RESULT, color::green),
		std::make_pair(COLOR::CARET, color::white),
		std::make_pair(COLOR::EQUALS, color::white),
		std::make_pair(COLOR::BRACKET, color::orange),
	};
} Global;

INLINE std::pair<std::string, std::string> getOperationResult(std::string const& rawInput)
{
	static const std::regex rgxParseOperation{ "(?:\\({0,1}([\\d\\^]+)\\){0,1}?)\\s*\\^\\s*(?:\\({0,1}([\\d\\^]+)\\){0,1}?)", std::regex_constants::optimize };
	std::smatch matches;

	if (!std::regex_search(rawInput, matches, rgxParseOperation))
		throw make_exception("Unrecognized operation syntax '", rawInput, "'");

	if (!matches[1].matched && !matches[2].matched)
		throw make_exception("Missing operand and exponent in '", rawInput, "'");
	else if (!matches[1].matched)
		throw make_exception("Missing operand in '", rawInput, "'");
	else if (!matches[2].matched)
		throw make_exception("Missing exponent in '", rawInput, "'");

	std::string num{ matches[1] }, exp{ matches[2] };
	std::string numEq, expEq;

	// resolve sub-expressions, if necessary
	if (num.find('^') != std::string::npos) {
		const auto& [eq, result] {getOperationResult(num) };
		numEq = eq;
		num = result;
	}
	if (exp.find('^') != std::string::npos) {
		const auto& [eq, result] { getOperationResult(exp) };
		expEq = eq;
		exp = result;
	}
	
	std::string result{ Pow(num, exp).getResultString() };

	std::stringstream ss;


	if (!Global.quiet) {
		// NUMBER:
		if (numEq.empty())
			ss << Global.colors(COLOR::NUMBER) << num << Global.colors();
		else {
			ss << Global.colors(COLOR::BRACKET) << '(' << Global.colors();
			ss << Global.colors(COLOR::NUMBER) << numEq << Global.colors();
			ss << Global.colors(COLOR::BRACKET) << ')' << Global.colors();
		}
		// CARET:
		ss << ' ' << Global.colors(COLOR::CARET) << '^' << Global.colors() << ' ';
		// EXPONENT:
		if (expEq.empty())
			ss << Global.colors(COLOR::EXPONENT) << exp << Global.colors();
		else {
			ss << Global.colors(COLOR::BRACKET) << '(' << Global.colors();
			ss << Global.colors(COLOR::EXPONENT) << expEq << Global.colors();
			ss << Global.colors(COLOR::BRACKET) << ')' << Global.colors();
		}
		// EQUALS:
		ss << ' ' << Global.colors(COLOR::EQUALS) << '=' << Global.colors() << ' ';
	}

	ss << Global.colors(COLOR::RESULT) << result << Global.colors();

	return { ss.str(), result };
}

int main(const int argc, char** argv)
{
	try {
		std::cout << term::EnableANSI;

		opt::ParamsAPI2 args{ argc, argv, opt::ArgumentParsingRules{ false } };
		const auto& [programPath, programName] {env::PATH().resolve_split(argv[0])};

		Global.quiet = args.check_any<opt::Flag, opt::Option>('q', "quiet");
		Global.colors.setActive(!args.check_any<opt::Flag, opt::Option>('n', "no-color"));

		if (args.check_any<opt::Flag, opt::Option>('h', "help")) {
			std::cout << Help(programName.generic_string()) << std::endl;
			return 0;
		}
		else if (args.check_any<opt::Flag, opt::Option>('v', "version")) {
			if (!Global.quiet)
				std::cout << "pow  v";
			std::cout << pow_VERSION_EXTENDED << std::endl;
			return 0;
		}

		const auto& operations{ str::split_all(str::join(args.typegetv_all<opt::Parameter>(), ' '), ',') };

		for (const auto& arg : operations) {
			const auto& [eq, result] {getOperationResult(arg)};
			std::cout << eq << std::endl;
		}

		return 0;
	} catch (const std::exception& ex) {
		std::cerr << Global.colors.get_fatal() << ex.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << Global.colors.get_fatal() << "An undefined exception occurred!" << std::endl;
		return 1;
	}
}
