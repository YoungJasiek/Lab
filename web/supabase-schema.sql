-- ============================================================================
-- Lab Developer Community - Supabase PostgreSQL Schema & Security Policies
-- Run this script in your Supabase Project -> SQL Editor
-- ============================================================================

-- 1. Create wiki_articles table
CREATE TABLE IF NOT EXISTS public.wiki_articles (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    slug TEXT NOT NULL UNIQUE,
    title TEXT NOT NULL,
    wikitext TEXT NOT NULL,
    updated_by UUID REFERENCES auth.users(id) ON DELETE SET NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 2. Create index on slug for fast lookups
CREATE INDEX IF NOT EXISTS idx_wiki_articles_slug ON public.wiki_articles(slug);

-- 3. Enable PostgreSQL Row Level Security (RLS)
ALTER TABLE public.wiki_articles ENABLE ROW LEVEL SECURITY;

-- 4. Security Policy: Anyone (authenticated or public) can READ articles
DROP POLICY IF EXISTS "Public can view wiki articles" ON public.wiki_articles;
CREATE POLICY "Public can view wiki articles"
ON public.wiki_articles
FOR SELECT
TO public
USING (true);

-- 5. Security Policy: Only verified ADMIN, DEV, or MODERATOR roles can INSERT/UPDATE
DROP POLICY IF EXISTS "Privileged roles can modify wiki articles" ON public.wiki_articles;
CREATE POLICY "Privileged roles can modify wiki articles"
ON public.wiki_articles
FOR ALL
TO authenticated
USING (
    (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
)
WITH CHECK (
    (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
);

-- ============================================================================
-- HELPER: How to promote a user to admin or dev in Supabase SQL Editor:
--
-- UPDATE auth.users
-- SET raw_app_meta_data = raw_app_meta_data || '{"role": "admin"}'::jsonb
-- WHERE email = 'your-email@example.com';
-- ============================================================================

-- 6. Create community_posts table (Lab Ecosystem Discussion Hub)
CREATE TABLE IF NOT EXISTS public.community_posts (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    flair TEXT NOT NULL DEFAULT 'General',
    author_id UUID REFERENCES auth.users(id) ON DELETE SET NULL,
    author_name TEXT NOT NULL DEFAULT 'Developer',
    author_role TEXT NOT NULL DEFAULT 'user',
    upvotes INT DEFAULT 1,
    is_pinned BOOLEAN DEFAULT false,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_community_posts_created_at ON public.community_posts(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_community_posts_flair ON public.community_posts(flair);

ALTER TABLE public.community_posts ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "Public can view community posts" ON public.community_posts;
CREATE POLICY "Public can view community posts"
ON public.community_posts
FOR SELECT
TO public
USING (true);

DROP POLICY IF EXISTS "Authenticated users can create posts" ON public.community_posts;
CREATE POLICY "Authenticated users can create posts"
ON public.community_posts
FOR INSERT
TO authenticated
WITH CHECK (auth.uid() IS NOT NULL);

DROP POLICY IF EXISTS "Authors and privileged roles can update posts" ON public.community_posts;
CREATE POLICY "Authors and privileged roles can update posts"
ON public.community_posts
FOR UPDATE
TO authenticated
USING (
    auth.uid() = author_id
    OR (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
);

DROP POLICY IF EXISTS "Authors and privileged roles can delete posts" ON public.community_posts;
CREATE POLICY "Authors and privileged roles can delete posts"
ON public.community_posts
FOR DELETE
TO authenticated
USING (
    auth.uid() = author_id
    OR (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
);

-- 7. Create community_comments table
CREATE TABLE IF NOT EXISTS public.community_comments (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    post_id UUID REFERENCES public.community_posts(id) ON DELETE CASCADE NOT NULL,
    parent_id UUID REFERENCES public.community_comments(id) ON DELETE CASCADE,
    author_id UUID REFERENCES auth.users(id) ON DELETE SET NULL,
    author_name TEXT NOT NULL DEFAULT 'Developer',
    author_role TEXT NOT NULL DEFAULT 'user',
    content TEXT NOT NULL,
    upvotes INT DEFAULT 1,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_community_comments_post_id ON public.community_comments(post_id);

ALTER TABLE public.community_comments ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "Public can view community comments" ON public.community_comments;
CREATE POLICY "Public can view community comments"
ON public.community_comments
FOR SELECT
TO public
USING (true);

DROP POLICY IF EXISTS "Authenticated users can create comments" ON public.community_comments;
CREATE POLICY "Authenticated users can create comments"
ON public.community_comments
FOR INSERT
TO authenticated
WITH CHECK (auth.uid() IS NOT NULL);

DROP POLICY IF EXISTS "Authors and privileged roles can delete comments" ON public.community_comments;
CREATE POLICY "Authors and privileged roles can delete comments"
ON public.community_comments
FOR DELETE
TO authenticated
USING (
    auth.uid() = author_id
    OR (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
);
