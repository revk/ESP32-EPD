#!/bin/csh -f
if($?QUERY_STRING) then
	setenv UPRN "$QUERY_STRING"
else
	setenv UPRN "$1"
endif
if(! "$UPRN") then
	echo Specify UPRN
	exit 1
endif
setenv TMP `mktemp`
setenv COOKIE `mktemp`
curl -s -L -c "$COOKIE" -b "$COOKIE" "https://maps.monmouthshire.gov.uk/?action=SetAddress&UniqueId=$UPRN" | /projects/tools/bin/htmlclean > "$TMP"
setenv BLACK `grep "^Household rubbish bag" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv RED `grep "^Red &amp; purple recycling bags" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv PURPLE `grep "^Red &amp; purple recycling bags" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv BLUE `grep "^Blue food bin" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv GREEN `grep "^Green Glass Box" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv GARDEN `grep "^Garden Waste Bins" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
setenv YELLOW `grep "^Yellow nappy" -A8 "$TMP" |grep -v '^<'|tail -1|sed -e "s/st / /" -e "s/nd / /" -e "s/rd / /" -e "s/th / /"`
if(`date +%b` == Dec) then
	if("$BLACK" =~ *January*) setenv BLACK "$BLACK next year"
	if("$RED" =~ *January*) setenv RED "$RED next year"
	if("$PURPLE" =~ *January*) setenv PURPLE "$PURPLE next year"
	if("$BLUE" =~ *January*) setenv BLUE "$BLUE next year"
	if("$GREEN" =~ *January*) setenv GREEN "$GREEN next year"
	if("$GARDEN" =~ *January*) setenv GARDEN "$GARDEN next year"
	if("$YELLOW" =~ *January*) setenv YELLOW "$YELLOW next year"
endif
rm -f "$COOKIE" "$TMP"

setenv DAY "$BLACK"
if("$DAY" == "" || ("$RED" != "" && `date +%s -d "$RED"` < `date +%s -d "$DAY"`)) setenv DAY "$RED"
if("$DAY" == "" || ("$PURPLE" != "" && `date +%s -d "$PURPLE"` < `date +%s -d "$DAY"`)) setenv DAY "$PURPLE"
if("$DAY" == "" || ("$BLUE" != "" && `date +%s -d "$BLUE"` < `date +%s -d "$DAY"`)) setenv DAY "$BLUE"
if("$DAY" == "" || ("$GREEN" != "" && `date +%s -d "$GREEN"` < `date +%s -d "$DAY"`)) setenv DAY "$GREEN"
if("$DAY" == "" || ("$GARDEN" != "" && `date +%s -d "$GARDEN"` < `date +%s -d "$DAY"`)) setenv DAY "$GARDEN"
if("$DAY" == "" || ("$YELLOW" != "" && `date +%s -d "$YELLOW"` < `date +%s -d "$DAY"`)) setenv DAY "$YELLOW"

if($?QUERY_STRING) then
echo "Content-Type: application/json"
echo ""
endif

echo "{"
if("$DAY" != "") then
echo '"collect":"'`date +%F -d "$DAY"`' 07:00:00",'
echo '"clear":"'`date +%F -d "$DAY"`' 12:00:00",'
echo '"bins":['
if("$BLACK" == "$DAY") echo '{"name":"BLACK","colour":"K","icon":"Black.png"},'
if("$RED" == "$DAY") echo '{"name":"RED","colour":"R","icon":"Red.png"},'
if("$PURPLE" == "$DAY") echo '{"name":"PURPLE","colour":"M","icon":"Purple.png"},'
if("$BLUE" == "$DAY") echo '{"name":"BLUE","colour":"B","icon":"Blue.png"},'
if("$GREEN" == "$DAY") echo '{"name":"GREEN","colour":"G","icon":"Green.png"},'
if("$GARDEN" == "$DAY") echo '{"name":"GARDEN","colour":"K","icon":"Garden.png"},'
if("$YELLOW" == "$DAY") echo '{"name":"YELLOW","colour":"Y","icon":"Yellow.png"},'
echo "{}"
echo "]"
endif
echo "}"
