#include "../inc/game_over_state.hpp"
#include "../inc/game_state.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace Engine
{
GameOverState::GameOverState( gameDataRef data, uint32 score )
    : m_Data( data ), m_Score( score ), m_BestScore( score ) {
	std::ifstream file( BEST_SCORE_FILE_PATH, std::ios::in | std::ios::binary );
	file.read( reinterpret_cast<char *>( &m_BestScore ), sizeof m_BestScore );
	file.close( );
}
GameOverState::~GameOverState( ) {}

void GameOverState::Init( ) {
	sf::Sprite bg, title, body, retry;

	AddTexture( "game_over_state_bg", GAME_OVER_BG_FILE_PATH, bg );
	AddTexture( "game_over_state_title", GAME_OVER_TITLE_FILE_PATH, title );
	AddTexture( "game_over_state_body", GAME_OVER_BODY_FILE_PATH, body );
	AddTexture( "game_over_state_retry", PLAY_BUTTON_FILE_PATH, retry );

	sf::Vector2u win_dim = m_Data->window.getSize( );
	float        win_x   = static_cast<float>( win_dim.x );
	float        win_y   = static_cast<float>( win_dim.y );

	body.setPosition( win_x / 2 - body.getGlobalBounds( ).width / 2,
	                  win_y / 2 - body.getGlobalBounds( ).height / 2 );
	title.setPosition( win_x / 2 - title.getGlobalBounds( ).width / 2,
	                   body.getPosition( ).y - title.getGlobalBounds( ).height * 1.3f );
	retry.setPosition( win_x / 2 - retry.getGlobalBounds( ).width / 2,
	                   body.getPosition( ).y + retry.getGlobalBounds( ).height * 2.f );

	m_Sprites.at( 0 ) = bg;
	m_Sprites.at( 1 ) = title;
	m_Sprites.at( 2 ) = body;
	m_Sprites.at( 3 ) = retry;

	SpecifyScoreText( body );
	SetMedal( body );
}

void GameOverState::HandleInput( ) {
	sf::Event event;
	while ( m_Data->window.pollEvent( event ) ) {
		if ( sf::Event::Closed == event.type ) m_Data->window.close( );
		if ( m_Data->input.IsSpriteClicked( m_Sprites.at( 3 ), sf::Mouse::Left,
		                                    m_Data->window ) )
			m_Data->machine.AddState( std::make_unique<GameState>( m_Data ) );
		else if ( sf::Event::KeyPressed == event.type ) {
			if ( event.key.code == sf::Keyboard::Space )
				m_Data->machine.AddState( std::make_unique<GameState>( m_Data ) );
			else if ( event.key.code == sf::Keyboard::Escape )
				m_Data->window.close( );
		}
	}
}

void GameOverState::Update( float frame_time ) {}

void GameOverState::Draw( float frame_time ) {
	m_Data->window.clear( );

	for ( const sf::Sprite &sprite : m_Sprites ) m_Data->window.draw( sprite );
	m_Data->window.draw( m_ScoreText );
	m_Data->window.draw( m_BestScoreText );
	m_Data->window.draw( m_Medal );

	m_Data->window.display( );
}

/*** Private methods ***/

void GameOverState::SetMedal( const sf::Sprite body ) {
	std::string medal_path;
	if ( m_Score > PLATINUM_SCORE )
		medal_path = PLATINUM_MEDALE_FILE_PATH;
	else if ( m_Score > GOLD_SCORE )
		medal_path = GOLD_MEDALE_FILE_PATH;
	else if ( m_Score > SILVER_SCORE )
		medal_path = SILVER_MEDALE_FILE_PATH;
	else
		medal_path = BRONZE_MEDALE_FILE_PATH;

	AddTexture( "medal", medal_path.c_str( ), m_Medal );

	const sf::FloatRect body_dim  = body.getGlobalBounds( ),
	                    medal_dim = m_Medal.getGlobalBounds( );

	m_Medal.setOrigin( medal_dim.width / 2, medal_dim.height / 2 );
	m_Medal.setPosition( body_dim.left + body_dim.width * .23f,
	                     body_dim.top + body_dim.height * .55f );
}

void GameOverState::SpecifyScoreText( const sf::Sprite body ) {
	const sf::FloatRect body_dim = body.getGlobalBounds( );

	m_ScoreText.setFont( m_Data->asset.GetFont( "flappy_font" ) );
	m_ScoreText.setCharacterSize( 55 );
	m_ScoreText.setFillColor( sf::Color::White );
	sf::FloatRect score_text_dim = m_ScoreText.getGlobalBounds( );
	m_ScoreText.setOrigin( score_text_dim.width / 2, score_text_dim.height / 2 );
	m_ScoreText.setPosition( body_dim.left + body_dim.width * .75f,
	                         body_dim.top + body_dim.height * .28f );
	m_ScoreText.setString( std::to_string( m_Score ) );

	m_BestScoreText.setFont( m_Data->asset.GetFont( "flappy_font" ) );
	m_BestScoreText.setCharacterSize( 60 );
	m_BestScoreText.setFillColor( sf::Color::White );
	sf::FloatRect best_score_text_dim = m_BestScoreText.getGlobalBounds( );
	m_BestScoreText.setOrigin( best_score_text_dim.width / 2,
	                           best_score_text_dim.height / 2 );
	m_BestScoreText.setPosition( body_dim.left + body_dim.width * .75f,
	                             body_dim.top + body_dim.height * .62f );
	m_BestScoreText.setString( std::to_string( m_BestScore ) );
}

void GameOverState::AddTexture( const char *tex_name, const char *file_path,
                                sf::Sprite &sprite ) {
	LoadTexture( tex_name, file_path );
	sprite.setTexture( m_Data->asset.GetTexture( tex_name ) );
}

void GameOverState::LoadTexture( const char *tex_name, const char *file_path ) {
	m_Data->asset.LoadTexture( tex_name, file_path );
}

}  // namespace Engine
