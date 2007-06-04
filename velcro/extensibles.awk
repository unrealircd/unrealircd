
BEGIN {
	printf "#include <map>\n"
}
{
	if ($1 == "@extensible")
	{
		system("cat ../velcro/extensibles.tmpl")
	}
	else 
	{
		print
	}
}

