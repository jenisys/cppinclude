#include "parser/impl/pr_parser_impl.hpp"

#include "parser/api/pr_include_file.hpp"
#include "parser/impl/pr_parser_context.hpp"

#include "fs/api/fs_file.hpp"

#include <fstream>
#include <vector>
#include <optional>
#include <cassert>

//------------------------------------------------------------------------------

namespace parser {

//------------------------------------------------------------------------------

Parser::IncludeFiles ParserImpl::parseFile( const fs::File & _file ) const
{
	IncludeFiles result;
	ParserContext context;

	while( !_file.eof() )
	{
		const std::string line = _file.getLine();
		context.setCurrentLine( line );
		auto includeFileOpt = parseLine( context );
		if( includeFileOpt )
		{
			result.push_back( *includeFileOpt );
		}
		context.increaseLineNumber();
	}

	return result;
}

//------------------------------------------------------------------------------

ParserImpl::IncludeFileOpt ParserImpl::parseLine( ParserContext & _context )
{
	constexpr size_t minimumSize = std::string_view{"/*"}.size() - 1;

	IncludeFileOpt resutl;

	const std::string & line = _context.getCurrentLine();
	const size_t size = line.size();
	if( size <= minimumSize )
		return resutl;

	size_t startPos = getStartPos( _context );

	for( size_t i = startPos; i < size; ++i )
	{
		const char str_char = line[i];
		switch( str_char )
		{
			case '/' :
				i = findComentEnd( _context, i);
				break;

			case '\"':
				i = findEndOfString( _context, i );
				break;

			case '#' :
			if( auto indexOpt = findInclude( line, i ); indexOpt )
				return parseInclude( _context, *indexOpt );
			else // if found # and it's not include it doesn't need analyze
				return resutl;
		}
	}

	return resutl;
}

//------------------------------------------------------------------------------

std::size_t ParserImpl::getStartPos( ParserContext & _context ) noexcept
{
	if( _context.isEnableMultilineComment() )
	{
		const std::size_t index = 0;
		return findComentEnd( _context, index );
	}

	if( _context.isEnableMultilineString() )
	{
		const std::size_t index = 0;
		return findEndOfString( _context, index );
	}

	return 0;
}

//------------------------------------------------------------------------------

std::size_t ParserImpl::findComentEnd( ParserContext & _context, std::size_t _index ) noexcept
{
	const size_t nextIndex = _context.isEnableMultilineComment() ? 0 : _index + 1;
	const std::string & line = _context.getCurrentLine();
	const size_t size = line.size();
	if( nextIndex >= size )
		return size;

	const char nextChar = line[ nextIndex ];

	if( !_context.isEnableMultilineComment() )
	{
		if( nextChar == '/' )
			return size;

		if( nextChar != '*' )
			return _index;

		assert( nextChar == '*' );
	}

	const auto pos = line.find( "*/", nextIndex );
	_context.setMultilineComment( pos == std::string::npos );

	return _context.isEnableMultilineComment() ? size : pos + 1;
}

//------------------------------------------------------------------------------

std::size_t ParserImpl::findEndOfString(
	ParserContext & _context,
	std::size_t _index
) noexcept
{
	const std::string & line = _context.getCurrentLine();
	const size_t size = line.size();
	for( size_t i = _index + 1 ; i < size ; ++i )
	{
		const char currentChar = line[i];
		switch ( currentChar )
		{
			case '\"':
			{
				_context.setMultilineString( false );
				return  i;
			}

			case '\\':
			{
				if( i == size - 1 )
				{
					_context.setMultilineString( true );
					return size;
				}
				else
				{
					const size_t nextPost = i + 1;
					assert( nextPost < size );
					if( nextPost < size && line[ nextPost ] == '\"' )
						i = nextPost;
				}
			}

		}
	}

	return size;
}

//------------------------------------------------------------------------------

std::optional< std::size_t > ParserImpl::findInclude(
	std::string_view _line,
	std::size_t _index
)
{
	static const std::string includePhase{ "include" };
	static const size_t includePhaseSize = includePhase.size();

	const size_t size = _line.size();
	bool isStartedCheckPhase = false;
	size_t indexPhase = 0;

	for( size_t i = _index + 1; i < size; ++i )
	{
		const char currentChar = _line[ i ];
		if( currentChar == ' ' || currentChar == '\t' )
		{
			if( isStartedCheckPhase )
				return std::nullopt;
		}
		else
		{
			isStartedCheckPhase = true;
			const char includeChar = includePhase[indexPhase];
			if( currentChar == includeChar )
			{
				++indexPhase;
				if( indexPhase >= includePhaseSize )
					return i;
			}
			else
			{
				return std::nullopt;
			}
		}
	}

	return std::nullopt;
}

//------------------------------------------------------------------------------

ParserImpl::IncludeFileOpt ParserImpl::parseInclude(
	const ParserContext & _context,
	std::size_t _index
)
{
	const std::string & line = _context.getCurrentLine();
	const size_t startPosSystem = line.find( '<', _index );
	const size_t startPosUser	= line.find( '"', _index );

	if( startPosSystem == std::string::npos && startPosUser == std::string::npos )
	{
		// it's strange that after #include don't exist < or "
		return std::nullopt;
	}

	const bool isSystem = startPosSystem != std::string::npos;
	const size_t startPosName = ( isSystem ? startPosSystem : startPosUser ) + 1;

	const char endChar = isSystem ? '>' : '"';

	size_t endPosName = line.find( endChar, startPosName );
	if( endPosName == std::string::npos )
	{
		// it's strange, include isn't closed
		return std::nullopt;
	}

	const std::string name = line.substr(
		startPosName,
		endPosName - startPosName
	);

	IncludeFileLocation location{
		_context.getLineNumber(),
		startPosName + 1, endPosName + 1
	};
	return IncludeFile{ location, name, isSystem };
}

//------------------------------------------------------------------------------

}
