static void
test_subject_class_ownership(void)
{
    printf("\n[subject_class_ownership]\n");

    TEST("subject may own class field and use its func in general func/action");
    {
        const char *source =
            "class Item {\n"
            "    let name: String;\n"
            "    let damage: Int;\n"
            "    func Info(self) -> String {\n"
            "        return self.name + \" dmg:\" + ToString(self.damage);\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    let name: String;\n"
            "    let weapon: Item;\n"
            "    let hp: Int;\n"
            "    func ShowWeapon(self) -> String {\n"
            "        return name + \" holds \" + weapon.Info();\n"
            "    }\n"
            "    action Strike(self, target: Player) -> Int {\n"
            "        let dmg = weapon.damage;\n"
            "        target.hp = target.hp - dmg;\n"
            "        return dmg;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let sword = Item(\"Iron Sword\", 15);\n"
            "    let hero = Player(\"Hero\", sword, 100);\n"
            "    let goblin = Player(\"Goblin\", Item(\"Claw\", 5), 50);\n"
            "    Log(hero.ShowWeapon());\n"
            "    Log(hero.Strike(goblin));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
