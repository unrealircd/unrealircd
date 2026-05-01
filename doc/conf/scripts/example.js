/**
 * Example UnrealIRCd JavaScript Script
 * 
 * This demonstrates the various features of the JavaScript module.
 * Place this file in your unrealircd/conf/scripts/ directory.
 */

// ============================================================================
// COMMAND EXAMPLES
// ============================================================================

/**
 * Simple INFO command - shows information about the user
 */
registerCommand({
    name: 'JSINFO',
    handler: function() {
        sendNotice($client, '========================================');
        sendNotice($client, 'Your Information:');
        sendNotice($client, 'Nickname: ' + $client.name);
        sendNotice($client, 'Username: ' + $client.username);
        sendNotice($client, 'IP Address: ' + $client.ip);
        sendNotice($client, 'Real Host: ' + $client.realhost);
        
        if ($client.vhost) {
            sendNotice($client, 'Virtual Host: ' + $client.vhost);
        }
        
        if ($client.account) {
            sendNotice($client, 'Account: ' + $client.account);
        }
        
        sendNotice($client, 'Is Oper: ' + ($client.isOper ? 'Yes' : 'No'));
        sendNotice($client, '========================================');
        
        log('User ' + $client.name + ' used JSINFO command');
        return 1;
    }
});

/**
 * FORTUNE command - sends a random fortune cookie message
 */
registerCommand({
    name: 'FORTUNE',
    handler: function() {
        var fortunes = [
            'A pleasant surprise is waiting for you.',
            'Your creativity will lead to success.',
            'A fresh start will put you on your way.',
            'Good things come to those who wait.',
            'The journey of a thousand miles begins with a single step.',
            'Your hard work will soon pay off.',
            'Opportunity knocks softly, listen carefully.',
            'You will make many changes before settling down.',
            'Success is going from failure to failure without loss of enthusiasm.',
            'The best time to plant a tree was 20 years ago. The second best time is now.',
            'Do not wait for the perfect moment. Take the moment and make it perfect.',
            'What you seek is seeking you.',
            'Every expert was once a beginner.',
            'The only way to do great work is to love what you do.'
        ];
        
        var randomIndex = Math.floor(Math.random() * fortunes.length);
        var fortune = fortunes[randomIndex];
        
        sendNotice($client, 'Fortune Cookie: ' + fortune);
        return 1;
    }
});

/**
 * DICE command - roll dice (e.g., /DICE 2d6)
 */
registerCommand({
    name: 'DICE',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /DICE <dice> (e.g., /DICE 2d6)');
            return 1;
        }
        
        var diceStr = $parv[0];
        var match = diceStr.match(/^(\d+)d(\d+)$/i);
        
        if (!match) {
            sendNotice($client, 'Invalid format. Use: XdY (e.g., 2d6 for 2 six-sided dice)');
            return 1;
        }
        
        var numDice = parseInt(match[1]);
        var numSides = parseInt(match[2]);
        
        if (numDice < 1 || numDice > 100) {
            sendNotice($client, 'Number of dice must be between 1 and 100');
            return 1;
        }
        
        if (numSides < 2 || numSides > 1000) {
            sendNotice($client, 'Number of sides must be between 2 and 1000');
            return 1;
        }
        
        var results = [];
        var total = 0;
        
        for (var i = 0; i < numDice; i++) {
            var roll = Math.floor(Math.random() * numSides) + 1;
            results.push(roll);
            total += roll;
        }
        
        sendNotice($client, 'Rolling ' + numDice + 'd' + numSides + '...');
        sendNotice($client, 'Results: [' + results.join(', ') + ']');
        sendNotice($client, 'Total: ' + total);
        
        return 1;
    }
});

/**
 * COINFLIP command - flip a coin
 */
registerCommand({
    name: 'COINFLIP',
    handler: function() {
        var result = Math.random() < 0.5 ? 'HEADS' : 'TAILS';
        sendNotice($client, 'Coin flip: ' + result + '!');
        return 1;
    }
});

/**
 * 8BALL command - magic 8-ball
 */
registerCommand({
    name: '8BALL',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /8BALL <question>');
            return 1;
        }
        
        var answers = [
            // Positive
            'It is certain.',
            'It is decidedly so.',
            'Without a doubt.',
            'Yes - definitely.',
            'You may rely on it.',
            'As I see it, yes.',
            'Most likely.',
            'Outlook good.',
            'Yes.',
            'Signs point to yes.',
            // Neutral
            'Reply hazy, try again.',
            'Ask again later.',
            'Better not tell you now.',
            'Cannot predict now.',
            'Concentrate and ask again.',
            // Negative
            'Don\'t count on it.',
            'My reply is no.',
            'My sources say no.',
            'Outlook not so good.',
            'Very doubtful.'
        ];
        
        var question = $parv.join(' ');
        var answer = answers[Math.floor(Math.random() * answers.length)];
        
        sendNotice($client, 'Question: ' + question);
        sendNotice($client, 'Magic 8-Ball says: ' + answer);
        return 1;
    }
});

/**
 * CALC command - simple calculator
 */
registerCommand({
    name: 'CALC',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /CALC <expression> (e.g., /CALC 2+2*3)');
            return 1;
        }
        
        var expr = $parv.join('');
        
        // Only allow safe characters for math expressions
        if (!/^[\d\s\+\-\*\/\.\(\)]+$/.test(expr)) {
            sendNotice($client, 'Invalid characters in expression. Only numbers and +-*/() allowed.');
            return 1;
        }
        
        try {
            // Use Function instead of eval for slightly better safety
            var result = new Function('return ' + expr)();
            if (typeof result === 'number' && isFinite(result)) {
                sendNotice($client, expr + ' = ' + result);
            } else {
                sendNotice($client, 'Error: Invalid result');
            }
        } catch (e) {
            sendNotice($client, 'Error evaluating expression: ' + e.message);
        }
        
        return 1;
    }
});

/**
 * CHOOSE command - pick randomly from options
 */
registerCommand({
    name: 'CHOOSE',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /CHOOSE option1, option2, option3...');
            return 1;
        }
        
        var input = $parv.join(' ');
        var options = input.split(/,\s*|\s+or\s+/i).filter(function(o) { return o.trim().length > 0; });
        
        if (options.length < 2) {
            sendNotice($client, 'Please provide at least 2 options separated by commas or "or"');
            return 1;
        }
        
        var choice = options[Math.floor(Math.random() * options.length)].trim();
        sendNotice($client, 'I choose: ' + choice);
        return 1;
    }
});

/**
 * ROT13 command - encode/decode text with ROT13
 */
registerCommand({
    name: 'ROT13',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /ROT13 <text>');
            return 1;
        }
        
        var text = $parv.join(' ');
        var result = text.replace(/[a-zA-Z]/g, function(c) {
            var base = c <= 'Z' ? 65 : 97;
            return String.fromCharCode((c.charCodeAt(0) - base + 13) % 26 + base);
        });
        
        sendNotice($client, 'ROT13: ' + result);
        return 1;
    }
});

/**
 * REVERSE command - reverse text
 */
registerCommand({
    name: 'REVERSE',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /REVERSE <text>');
            return 1;
        }
        
        var text = $parv.join(' ');
        var result = text.split('').reverse().join('');
        
        sendNotice($client, 'Reversed: ' + result);
        return 1;
    }
});

// ============================================================================
// HTTP API EXAMPLES - These use async callbacks
// ============================================================================

// Store pending requests with their client info
var pendingRequests = {};

/**
 * CATFACT command - get a random cat fact from an API
 */
registerCommand({
    name: 'CATFACT',
    handler: function() {
        var clientName = $client.name;
        pendingRequests['catfact_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching a cat fact...');
        httpGet('https://catfact.ninja/fact', 'onCatFactResponse');
        return 1;
    }
});

function onCatFactResponse(error, body) {
    // Find which clients requested this
    for (var key in pendingRequests) {
        if (key.indexOf('catfact_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching cat fact: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.fact) {
                        sendNotice(client, 'Cat Fact: ' + data.fact);
                    } else {
                        sendNotice(client, 'Could not parse cat fact response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

/**
 * JOKE command - get a random joke
 */
registerCommand({
    name: 'JOKE',
    handler: function() {
        var clientName = $client.name;
        pendingRequests['joke_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching a joke...');
        httpGet('https://official-joke-api.appspot.com/random_joke', 'onJokeResponse');
        return 1;
    }
});

function onJokeResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('joke_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching joke: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.setup && data.punchline) {
                        sendNotice(client, 'Joke: ' + data.setup);
                        sendNotice(client, '... ' + data.punchline);
                    } else {
                        sendNotice(client, 'Could not parse joke response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

/**
 * DOGPIC command - get a random dog picture URL
 */
registerCommand({
    name: 'DOGPIC',
    handler: function() {
        var clientName = $client.name;
        pendingRequests['dogpic_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching a random dog picture...');
        httpGet('https://dog.ceo/api/breeds/image/random', 'onDogPicResponse');
        return 1;
    }
});

function onDogPicResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('dogpic_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching dog picture: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.message) {
                        sendNotice(client, 'Random Dog: ' + data.message);
                    } else {
                        sendNotice(client, 'Could not parse response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

/**
 * TRIVIA command - get a random trivia question
 */
registerCommand({
    name: 'TRIVIA',
    handler: function() {
        var clientName = $client.name;
        pendingRequests['trivia_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching a trivia question...');
        httpGet('https://opentdb.com/api.php?amount=1&type=multiple', 'onTriviaResponse');
        return 1;
    }
});

function onTriviaResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('trivia_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching trivia: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.results && data.results[0]) {
                        var q = data.results[0];
                        // Decode HTML entities
                        var question = decodeHtmlEntities(q.question);
                        var category = decodeHtmlEntities(q.category);
                        var correct = decodeHtmlEntities(q.correct_answer);
                        var answers = q.incorrect_answers.map(decodeHtmlEntities);
                        answers.push(correct);
                        // Shuffle answers
                        answers.sort(function() { return Math.random() - 0.5; });
                        
                        sendNotice(client, '[' + category + '] ' + question);
                        sendNotice(client, 'Options: ' + answers.join(' | '));
                        sendNotice(client, '(Answer hidden - use /TRIVIAANSWER to reveal)');
                        
                        // Store the answer
                        pendingRequests['answer_' + clientName] = correct;
                    } else {
                        sendNotice(client, 'Could not parse trivia response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

// Helper to decode HTML entities
function decodeHtmlEntities(text) {
    var entities = {
        '&amp;': '&',
        '&lt;': '<',
        '&gt;': '>',
        '&quot;': '"',
        '&#039;': "'",
        '&apos;': "'",
        '&ndash;': '-',
        '&mdash;': '--',
        '&hellip;': '...',
        '&eacute;': 'e',
        '&egrave;': 'e',
        '&uuml;': 'u',
        '&ouml;': 'o',
        '&auml;': 'a'
    };
    for (var entity in entities) {
        text = text.replace(new RegExp(entity, 'g'), entities[entity]);
    }
    return text;
}

/**
 * TRIVIAANSWER command - reveal the answer to the last trivia question
 */
registerCommand({
    name: 'TRIVIAANSWER',
    handler: function() {
        var answerKey = 'answer_' + $client.name;
        if (pendingRequests[answerKey]) {
            sendNotice($client, 'The answer was: ' + pendingRequests[answerKey]);
            delete pendingRequests[answerKey];
        } else {
            sendNotice($client, 'No pending trivia answer. Use /TRIVIA first!');
        }
        return 1;
    }
});

/**
 * WEATHER command - get weather for a location (using wttr.in)
 */
registerCommand({
    name: 'WEATHER',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /WEATHER <city>');
            return 1;
        }
        
        var city = $parv.join('+');
        var clientName = $client.name;
        pendingRequests['weather_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching weather for ' + $parv.join(' ') + '...');
        httpGet('https://wttr.in/' + city + '?format=j1', 'onWeatherResponse');
        return 1;
    }
});

function onWeatherResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('weather_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching weather: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.current_condition && data.current_condition[0]) {
                        var c = data.current_condition[0];
                        var area = data.nearest_area && data.nearest_area[0];
                        var location = area ? (area.areaName[0].value + ', ' + area.country[0].value) : 'Unknown';
                        
                        sendNotice(client, 'Weather for ' + location + ':');
                        sendNotice(client, 'Temperature: ' + c.temp_C + 'C (' + c.temp_F + 'F)');
                        sendNotice(client, 'Feels like: ' + c.FeelsLikeC + 'C (' + c.FeelsLikeF + 'F)');
                        sendNotice(client, 'Condition: ' + c.weatherDesc[0].value);
                        sendNotice(client, 'Humidity: ' + c.humidity + '% | Wind: ' + c.windspeedKmph + ' km/h ' + c.winddir16Point);
                    } else {
                        sendNotice(client, 'Could not parse weather response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

/**
 * QUOTE command - get a random inspirational quote
 */
registerCommand({
    name: 'QUOTE',
    handler: function() {
        var clientName = $client.name;
        pendingRequests['quote_' + clientName] = clientName;
        
        sendNotice($client, 'Fetching an inspirational quote...');
        httpGet('https://api.quotable.io/random', 'onQuoteResponse');
        return 1;
    }
});

function onQuoteResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('quote_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error fetching quote: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.content && data.author) {
                        sendNotice(client, '"' + data.content + '"');
                        sendNotice(client, '  -- ' + data.author);
                    } else {
                        sendNotice(client, 'Could not parse quote response');
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

/**
 * IP command - lookup IP geolocation info
 */
registerCommand({
    name: 'GEOIP',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /GEOIP <ip address>');
            return 1;
        }
        
        var ip = $parv[0];
        // Basic IP validation
        if (!/^[\d\.]+$/.test(ip) && !/^[a-f0-9:]+$/i.test(ip)) {
            sendNotice($client, 'Invalid IP address format');
            return 1;
        }
        
        var clientName = $client.name;
        pendingRequests['geoip_' + clientName] = clientName;
        
        sendNotice($client, 'Looking up IP: ' + ip + '...');
        httpGet('http://ip-api.com/json/' + ip, 'onGeoIPResponse');
        return 1;
    }
});

function onGeoIPResponse(error, body) {
    for (var key in pendingRequests) {
        if (key.indexOf('geoip_') === 0) {
            var clientName = pendingRequests[key];
            delete pendingRequests[key];
            
            var client = findClient(clientName);
            if (!client) continue;
            
            if (error) {
                sendNotice(client, 'Error looking up IP: ' + error);
            } else {
                try {
                    var data = JSON.parse(body);
                    if (data && data.status === 'success') {
                        sendNotice(client, 'GeoIP Info for ' + data.query + ':');
                        sendNotice(client, 'Country: ' + data.country + ' (' + data.countryCode + ')');
                        sendNotice(client, 'Region: ' + data.regionName);
                        sendNotice(client, 'City: ' + data.city);
                        sendNotice(client, 'ISP: ' + data.isp);
                        sendNotice(client, 'Timezone: ' + data.timezone);
                    } else {
                        sendNotice(client, 'GeoIP lookup failed: ' + (data.message || 'Unknown error'));
                    }
                } catch (e) {
                    sendNotice(client, 'Error parsing response: ' + e.message);
                }
            }
            break;
        }
    }
}

// ============================================================================
// HOOK EXAMPLES
// ============================================================================

/**
 * Welcome new users with a custom message
 */
registerHook({
    type: HOOKTYPE_LOCAL_CONNECT,
    handler: function() {
        sendNotice($client, '========================================');
        sendNotice($client, '   Welcome to our IRC Network!');
        sendNotice($client, '');
        sendNotice($client, '   Fun Commands:');
        sendNotice($client, '   /FORTUNE - Get a fortune cookie');
        sendNotice($client, '   /DICE 2d6 - Roll dice');
        sendNotice($client, '   /8BALL <question> - Ask the magic 8-ball');
        sendNotice($client, '   /JOKE - Get a random joke');
        sendNotice($client, '   /CATFACT - Learn about cats');
        sendNotice($client, '   /WEATHER <city> - Check the weather');
        sendNotice($client, '   /TRIVIA - Get a trivia question');
        sendNotice($client, '');
        sendNotice($client, '   Have fun and be respectful!');
        sendNotice($client, '========================================');
        
        log('New user connected: ' + $client.name + ' [' + $client.ip + ']');
        
        return 0; // Continue normal processing
    }
});

/**
 * Log when users quit
 */
registerHook({
    type: HOOKTYPE_LOCAL_QUIT,
    handler: function() {
        log('User quit: ' + $client.name + ' [' + $client.ip + ']');
        return 0;
    }
});

/**
 * Announce when someone becomes an oper
 */
registerHook({
    type: HOOKTYPE_LOCAL_OPER,
    handler: function() {
        log($client.name + ' is now an IRC Operator');
        sendNotice($client, 'You are now an IRC Operator. Use your powers wisely!');
        return 0;
    }
});

// ============================================================================
// STATS/INFO COMMANDS
// ============================================================================

/**
 * JSHELP command - list all JavaScript commands
 */
registerCommand({
    name: 'JSHELP',
    handler: function() {
        sendNotice($client, '=== JavaScript Commands ===');
        sendNotice($client, 'Fun & Games:');
        sendNotice($client, '  /FORTUNE - Get a fortune cookie message');
        sendNotice($client, '  /DICE <XdY> - Roll X dice with Y sides');
        sendNotice($client, '  /COINFLIP - Flip a coin');
        sendNotice($client, '  /8BALL <question> - Ask the magic 8-ball');
        sendNotice($client, '  /CHOOSE opt1, opt2... - Pick randomly');
        sendNotice($client, '');
        sendNotice($client, 'Utilities:');
        sendNotice($client, '  /CALC <expr> - Simple calculator');
        sendNotice($client, '  /ROT13 <text> - ROT13 encode/decode');
        sendNotice($client, '  /REVERSE <text> - Reverse text');
        sendNotice($client, '');
        sendNotice($client, 'API-Powered:');
        sendNotice($client, '  /JOKE - Get a random joke');
        sendNotice($client, '  /CATFACT - Get a cat fact');
        sendNotice($client, '  /DOGPIC - Get a random dog picture URL');
        sendNotice($client, '  /QUOTE - Get an inspirational quote');
        sendNotice($client, '  /WEATHER <city> - Get weather info');
        sendNotice($client, '  /TRIVIA - Get a trivia question');
        sendNotice($client, '  /TRIVIAANSWER - Reveal trivia answer');
        sendNotice($client, '  /GEOIP <ip> - Lookup IP geolocation');
        sendNotice($client, '');
        sendNotice($client, 'Timers:');
        sendNotice($client, '  /COUNTDOWN <seconds> - Start a countdown');
        sendNotice($client, '  /REMINDER <seconds> <message> - Set a reminder');
        sendNotice($client, '');
        sendNotice($client, 'Info:');
        sendNotice($client, '  /JSINFO - Show your connection info');
        sendNotice($client, '  /JSHELP - This help message');
        return 1;
    }
});

// Helper function for padding strings
function padRight(str, length) {
    while (str.length < length) {
        str += ' ';
    }
    return str;
}

// ============================================================================
// TIMER EXAMPLES
// ============================================================================

/**
 * COUNTDOWN command - countdown from N seconds to 0
 */
registerCommand({
    name: 'COUNTDOWN',
    handler: function() {
        if ($parc < 1) {
            sendNotice($client, 'Usage: /COUNTDOWN <seconds>');
            return 1;
        }
        
        var seconds = parseInt($parv[0]);
        if (isNaN(seconds) || seconds < 1 || seconds > 60) {
            sendNotice($client, 'Seconds must be between 1 and 60');
            return 1;
        }
        
        var clientName = $client.name;
        var remaining = seconds;
        
        sendNotice($client, '⏱️ Countdown started: ' + seconds + ' seconds');
        
        var timerId = setInterval(function() {
            remaining--;
            var target = findClient(clientName);
            
            if (!target) {
                clearInterval(timerId);
                return;
            }
            
            if (remaining > 0) {
                if (remaining <= 5 || remaining % 10 === 0) {
                    sendNotice(target, '⏱️ ' + remaining + ' seconds remaining...');
                }
            } else {
                sendNotice(target, '🎉 COUNTDOWN COMPLETE! 🎉');
                clearInterval(timerId);
            }
        }, 1000);
        
        return 1;
    }
});

/**
 * REMINDER command - set a reminder for X seconds from now
 */
registerCommand({
    name: 'REMINDER',
    handler: function() {
        if ($parc < 2) {
            sendNotice($client, 'Usage: /REMINDER <seconds> <message>');
            return 1;
        }
        
        var seconds = parseInt($parv[0]);
        if (isNaN(seconds) || seconds < 1 || seconds > 3600) {
            sendNotice($client, 'Seconds must be between 1 and 3600 (1 hour)');
            return 1;
        }
        
        // Join all remaining params as the message
        var message = $parv.slice(1).join(' ');
        var clientName = $client.name;
        
        sendNotice($client, '⏰ Reminder set for ' + seconds + ' seconds from now');
        
        setTimeout(function() {
            var target = findClient(clientName);
            if (target) {
                sendNotice(target, '⏰ REMINDER: ' + message);
            }
        }, seconds * 1000);
        
        return 1;
    }
});

// ============================================================================
// HOOK EXAMPLES
// ============================================================================

/**
 * Welcome message for local users
 */
registerHook({
    type: HOOKTYPE_LOCAL_CONNECT,
    handler: function() {
        sendNotice($client, '*** Welcome to the server, ' + $client.name + '!');
        sendNotice($client, '*** Type /JSHELP for a list of JavaScript commands.');
        log('Local connect: ' + $client.name + ' from ' + $client.ip);
        return 0;
    }
});

/**
 * Log channel joins
 */
registerHook({
    type: HOOKTYPE_LOCAL_JOIN,
    handler: function() {
        log('Join: ' + $client.name + ' -> ' + $channel.name);
        return 0;
    }
});

/**
 * Log quits
 */
registerHook({
    type: HOOKTYPE_LOCAL_QUIT,
    handler: function() {
        log('Quit: ' + $client.name + ' (' + $reason + ')');
        return 0;
    }
});

/**
 * Log nick changes
 */
registerHook({
    type: HOOKTYPE_LOCAL_NICKCHANGE,
    handler: function() {
        log('Nick change: ' + $client.name + ' -> ' + $newnick);
        return 0;
    }
});

/**
 * Log topic changes
 */
registerHook({
    type: HOOKTYPE_TOPIC,
    handler: function() {
        log('Topic in ' + $channel.name + ' set by ' + $client.name + ': ' + $topic);
        return 0;
    }
});

/**
 * Respond to channel messages containing "hello bot"
 */
registerHook({
    type: HOOKTYPE_CHANMSG,
    handler: function() {
        if ($isPrivmsg && $text.toLowerCase().indexOf('hello bot') !== -1) {
            // Find the client again since this is async-ish
            var target = findClient($client.name);
            if (target) {
                sendNotice(target, 'Hello, ' + $client.name + '! 👋');
            }
        }
        return 0;
    }
});

/**
 * Notify when someone goes away
 */
registerHook({
    type: HOOKTYPE_AWAY,
    handler: function() {
        if ($isAway) {
            log($client.name + ' is now away: ' + $reason);
        } else {
            log($client.name + ' is back');
        }
        return 0;
    }
});

/**
 * Log when someone opers up
 */
registerHook({
    type: HOOKTYPE_LOCAL_OPER,
    handler: function() {
        if ($isOper) {
            log($client.name + ' is now an IRC operator (class: ' + $operClass + ')');
        } else {
            log($client.name + ' is no longer an IRC operator');
        }
        return 0;
    }
});

// ============================================================================
// STARTUP
// ============================================================================

// Log that the script loaded successfully
log('Example JavaScript script loaded successfully!');
log('Commands: JSINFO, JSHELP, FORTUNE, DICE, COINFLIP, 8BALL, CHOOSE, CALC, ROT13, REVERSE');
log('API Commands: JOKE, CATFACT, DOGPIC, QUOTE, WEATHER, TRIVIA, GEOIP');
log('Timer Commands: COUNTDOWN, REMINDER');

// One-time delayed startup message
setTimeout(function() {
    log('JavaScript module timers are working!');
}, 3000);
