{
	if ($1 == "@use")
	{
		printf "\tif (!Velcro::instanceOf().dependsOnModule(\"%s\", \"%s\"))\n", Module, $2
		printf "\t\t{ *ret = -1; return; }\n"
	}
}
