package require sqlite3

set db [sqlite3.open :memory:]
$db query {CREATE TABLE plays (id, author, title)}
$db query {INSERT INTO plays (id, author, title) VALUES (1, 'Goethe', 'Faust');}
$db query {INSERT INTO plays (id, author, title) VALUES (2, 'Shakespeare', 'Hamlet');}
$db query {INSERT INTO plays (id, author, title) VALUES (3, 'Sophocles', 'Oedipus Rex');}
set res [$db query "SELECT * FROM plays"]
$db close
foreach r $res {puts $r(author)}
