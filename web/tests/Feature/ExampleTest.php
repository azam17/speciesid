<?php

namespace Tests\Feature;

// use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

class ExampleTest extends TestCase
{
    /**
     * A basic test example.
     */
    public function test_landing_page_is_public_and_dashboard_requires_login(): void
    {
        $this->get('/')
            ->assertOk()
            ->assertSee('SpeciesID')
            ->assertSee('Halal meat lab screening');

        $this->get('/features')
            ->assertOk()
            ->assertSee('SpeciesID screening')
            ->assertSee('Standard method');

        $this->get('/dashboard')
            ->assertRedirect('/login');
    }
}
